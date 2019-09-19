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

struct null_strand {
    null_strand(as::io_context& ioc) noexcept : ioc_(ioc) {}
    template <typename Func, typename Allocator>
    void post(Func&& f, Allocator) const {
        as::post(
            ioc_,
            [f = std::forward<Func>(f)] () mutable {
                std::forward<Func>(f)();
            }
        );
    }
    template <typename Func, typename Allocator>
    void defer(Func&& f, Allocator) const {
        as::defer(
            ioc_,
            [f = std::forward<Func>(f)] () mutable {
                std::forward<Func>(f)();
            }
        );
    }
    template <typename Func, typename Allocator>
    void dispatch(Func&& f, Allocator) const {
        std::forward<Func>(f)();
    }
    void on_work_started() const noexcept {}
    void on_work_finished() const noexcept {}
    as::io_context& context() noexcept{ return ioc_; }
    as::io_context const& context() const noexcept { return ioc_; }
private:
    as::io_context& ioc_;
};

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
