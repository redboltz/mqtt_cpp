// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_NULL_STRAND_HPP)
#define MQTT_NULL_STRAND_HPP

#include <utility>

#include <boost/asio.hpp>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

namespace detail {

struct null_strand {
    using inner_executor_type = as::io_context::executor_type;
    template <typename Executor>
    explicit null_strand(Executor e) noexcept
        : exe_{force_move(e)}
    {}

    template <typename Func, typename Allocator>
    void defer(Func&& f, Allocator) const {
        as::defer(
            exe_,
            [f = std::forward<Func>(f)] () mutable {
                std::move(f)();
            }
        );
    }
    template <typename Func, typename Allocator>
    void dispatch(Func&& f, Allocator) const {
        as::dispatch(
            exe_,
            [f = std::forward<Func>(f)] () mutable {
                std::move(f)();
            }
        );
    }
    template <typename Func, typename Allocator>
    void post(Func&& f, Allocator) const {
        as::post(
            exe_,
            [f = std::forward<Func>(f)] () mutable {
                std::move(f)();
            }
        );
    }
    as::any_io_executor get_inner_executor() const {
        return exe_;
    }
    void on_work_started() const noexcept {}
    void on_work_finished() const noexcept {}
    bool running_in_this_thread() const noexcept { return true; }
    operator as::any_io_executor() const {
        return exe_;
    }
private:
    as::io_context::executor_type exe_;
};

} // namespace detail

using null_strand = detail::null_strand;

inline bool operator==(null_strand const& lhs, null_strand const& rhs) {
    return std::addressof(lhs) == std::addressof(rhs);
}

inline bool operator!=(null_strand const& lhs, null_strand const& rhs) {
    return !(lhs == rhs);
}

} // namespace MQTT_NS

namespace boost {
namespace asio {

template<>
struct is_executor<MQTT_NS::null_strand> : std::true_type {
};

} // namespace asio
} // namespace boost

#endif // MQTT_NULL_STRAND_HPP
