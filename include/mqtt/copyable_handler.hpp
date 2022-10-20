// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_COPYABLE_HANDLER_HPP)
#define MQTT_COPYABLE_HANDLER_HPP

#include <type_traits>

#include <boost/asio.hpp>

#include <mqtt/namespace.hpp>
#include <mqtt/copyable_function.hpp>
#include <mqtt/move.hpp>
#include <mqtt/apply.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

template <typename Sig>
struct copyable_handler {
    using executor_type = as::any_io_executor;

    copyable_handler() = default;

    template <
        typename Func,
        typename std::enable_if_t<
            std::is_convertible<Func, copyable_function<Sig>>::value
        >* = nullptr
    >
    copyable_handler(Func&& f)
        : exe_{as::get_associated_executor(f)},
          func_{std::forward<Func>(f)}
    {
    }

    executor_type get_executor() const { return exe_; }

    template <typename... Params>
    void operator()(Params&&... params) {
        if (exe_ == as::system_executor()) {
            func_(std::forward<Params>(params)...);
            return;
        }
        as::dispatch(
            exe_,
            [func = func_, pt = std::tuple<Params...>(std::forward<Params>(params)...)] () mutable {
                MQTT_NS::apply(force_move(func), std::move(pt));
            }
        );
    }

    operator bool() const { return static_cast<bool>(func_); }

private:
    executor_type exe_;
    copyable_function<Sig> func_;
};

} // namespace MQTT_NS

#endif // MQTT_COPYABLE_HANDLER_HPP
