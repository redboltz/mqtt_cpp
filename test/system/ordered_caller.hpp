// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef MQTT_ORDERED_CALLER_HPP
#define MQTT_ORDERED_CALLER_HPP

#include <vector>
#include <string>
#include <functional>

struct ordered_caller {
    template <typename... Funcs>
    ordered_caller(std::size_t& index, Funcs&&... funcs)
        : index_ {index}, funcs_ { std::forward<Funcs>(funcs)... } {}
    bool operator()() {
        if (index_ >= funcs_.size()) {
            return false;
        }
        funcs_[index_++]();
        return true;
    }
private:
    std::size_t& index_;
    std::vector<std::function<void()>> funcs_;
};

static std::map<std::string, std::size_t> ordered_caller_fileline_to_index;

inline void clear_ordered() {
    ordered_caller_fileline_to_index.clear();
}

template <typename... Funcs>
auto make_ordered_caller(std::string file, std::size_t line, Funcs&&... funcs) {
    return
        ordered_caller{
            ordered_caller_fileline_to_index[file + std::to_string(line)],
            std::forward<Funcs>(funcs)...
        }();
}

#define MQTT_ORDERED(...)                                       \
    make_ordered_caller(__FILE__, __LINE__, __VA_ARGS__)


#endif // MQTT_ORDERED_CALLER_HPP
