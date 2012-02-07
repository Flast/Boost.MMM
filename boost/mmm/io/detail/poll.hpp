//          Copyright Kohei Takahashi 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MMM_IO_DETAIL_POLL_HPP
#define BOOST_MMM_IO_DETAIL_POLL_HPP

#include <boost/config.hpp>

#if !defined(BOOST_WINDOWS)
#   define BOOST_MMM_DETAIL_HAS_POLL
#endif

#include <boost/optional.hpp>
#include <boost/chrono/duration.hpp>

#include <cerrno>
#include <boost/system/error_code.hpp>

#include <sys/time.h>
#if defined(BOOST_MMM_DETAIL_HAS_POLL)
#include <sys/poll.h>
#else
#include <sys/select.h>
#endif

namespace boost { namespace mmm { namespace io { namespace detail {

template <typename ResultType>
inline ResultType
poll_result_handling(ResultType result, system::error_code &err_code)
{
    err_code = result < 0
      ? system::error_code(errno, system::system_category())
      : system::error_code();
    return result;
}

struct polling_events
{
#if defined(BOOST_MMM_DETAIL_HAS_POLL)
    BOOST_STATIC_CONSTEXPR int in  = POLLIN;
    BOOST_STATIC_CONSTEXPR int out = POLLOUT;
#else
    BOOST_STATIC_CONSTEXPR int in  = 1 << 0;
    BOOST_STATIC_CONSTEXPR int out = 1 << 1;
#endif
    BOOST_STATIC_CONSTEXPR int io  = in & out;
};

#if defined(BOOST_MMM_DETAIL_HAS_POLL)
using ::pollfd;
#else
struct pollfd
{
    int fd;
    int events;
    int revents;
};
#endif

template <typename Rep, typename Period>
inline int
poll_fds_impl(pollfd *fds, int count
, boost::chrono::duration<Rep, Period> *timeout
, system::error_code &err_code)
{
    using boost::chrono::duration_cast;
#if defined(BOOST_MMM_DETAIL_HAS_POLL)
    using boost::chrono::milliseconds;
    const int to = timeout ? duration_cast<milliseconds>(*timeout).count() : -1;
    return poll_result_handling(::poll(fds, count, to), err_code);
#else
    using boost::chrono::seconds;
    using boost::chrono::microseconds;

    timeval _to, *to = 0;
    if (timeout)
    {
        const seconds sec = duration_cast<seconds>(*timeout);
        _to.tv_sec  = sec.count();
        _to.tv_usec = duration_cast<microseconds>(*timeout - sec).count();
        to = &_to;
    }

    int nfds = -1;
    fd_set readfds;  FD_ZERO(&readfds);
    fd_set writefds; FD_ZERO(&writefds);
    for (int i = 0; i < count; ++i)
    {
        const pollfd &fd = fds[i];
        nfds = (nfds < fd.fd) ? fd.fd : nfds;
        if (fd.events & polling_events::in) { FD_SET(fd.fd, &readfds); }
        if (fd.events & polling_events::out) { FD_SET(fd.fd, &writefds); }
    }

    const int result = ::select(nfds + 1, &readfds, &writefds, 0, to);
    poll_result_handling(result, err_code);

    for (int i = 0; i < count; ++i)
    {
        pollfd &fd = fds[i];
        fd.revents = 0;
        if (fd.events & polling_events::in && FD_ISSET(fd.fd, &readfds))
        {
            fd.revents |= polling_events::in;
        }
        if (fd.events & polling_events::out && FD_ISSET(fd.fd, &writefds))
        {
            fd.revents |= polling_events::out;
        }
    }

    return result;
#endif
}

template <typename Rep, typename Period>
inline int
poll_fds(pollfd *fds, int count
, boost::chrono::duration<Rep, Period> timeout
, system::error_code &err_code)
{
    return poll_fds_impl(fds, count, &timeout, err_code);
}

inline int
poll_fds(pollfd *fds, int count, system::error_code &err_code)
{
    using boost::chrono::seconds;
    return poll_fds_impl(fds, count, static_cast<seconds *>(0), err_code);
}

} } } } // namespace boost::mmm::io::detail

#endif

