// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_MOVE_ONLY_HANDLER_HPP)
#define MQTT_MOVE_ONLY_HANDLER_HPP

#include <type_traits>

#include <boost/asio.hpp>
//#include <boost/hof/apply.hpp>
#include <mqtt/namespace.hpp>
#include <mqtt/move_only_function.hpp>
#include <mqtt/move.hpp>
#include <mqtt/apply.hpp>

namespace MQTT_NS {

namespace as = boost::asio;

template <typename Sig>
struct move_only_handler {
    using executor_type = as::any_io_executor;

    move_only_handler() = default;

    template <
        typename Func,
        std::enable_if_t<
            std::is_convertible<Func, move_only_function<Sig>>::value
        >* = nullptr
    >
    move_only_handler(Func&& f)
        : exe_{as::get_associated_executor(f)},
          func_{std::forward<Func>(f)}
    {
    }

    executor_type get_executor() const { return exe_; }

    template <typename... Params>
    void operator()(Params&&... params) const {
        as::dispatch(
            exe_,
            [this, pt = std::tuple<Params...>(std::forward<Params>(params)...)] () mutable {
                auto avoid_func_copy =
                    [&](auto&&... params) mutable {
                        func_(std::forward<decltype(params)>(params)...);
                    };
                MQTT_NS::apply(avoid_func_copy, std::move(pt));
                //std::apply(avoid_func_copy, std::move(pt));
            }
        );
    }

    template <typename... Params>
    void operator()(Params&&... params) {
        as::dispatch(
            exe_,
            [this, pt = std::tuple<Params...>(std::forward<Params>(params)...)] () mutable {
                auto avoid_func_copy =
                    [&](auto&&... params) mutable {
                        func_(std::forward<decltype(params)>(params)...);
                    };
                MQTT_NS::apply(avoid_func_copy, std::move(pt));
                //std::apply(avoid_func_copy, std::move(pt));
            }
        );
    }

    operator bool() const { return static_cast<bool>(func_); }

private:
    executor_type exe_;
    move_only_function<Sig> func_;
};

} // namespace MQTT_NS

#endif // MQTT_MOVE_ONLY_HANDLER_HPP
