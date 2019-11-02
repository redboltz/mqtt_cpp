// Copyright Takatoshi Kondo 2019
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_LOCKED_COUT_HPP)
#define MQTT_LOCKED_COUT_HPP

#include <mutex>
#include <iostream>

class locked_stream {
public:
    locked_stream(std::ostream& stream)
        :lock_(mtx_),
         stream_(stream) {}

    friend
    locked_stream&& operator<<(locked_stream&& s, std::ostream& (*arg)(std::ostream&)) {
        s.stream_ << arg;
        return std::move(s);
    }

    template <typename Arg>
    friend
    locked_stream&& operator<<(locked_stream&& s, Arg&& arg) {
        s.stream_ << std::forward<Arg>(arg);
        return std::move(s);
    }

private:
    static std::mutex mtx_;
    std::unique_lock<std::mutex> lock_;
    std::ostream& stream_;

};

std::mutex locked_stream::mtx_{};

inline
locked_stream locked_cout() {
    return locked_stream(std::cout);
}

#endif // MQTT_LOCKED_COUT_HPP
