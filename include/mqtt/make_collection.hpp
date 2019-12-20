#if !defined(MQTT_MAKE_COLLECTION_HPP)
#define MQTT_MAKE_COLLECTION_HPP

#include <functional>
#include <mqtt/namespace.hpp>
#include <utility>

namespace MQTT_NS {
#if defined(MQTT_CLANG_STL_WORKAROUND)

template <typename FIRST_T, typename SECOND_T, typename... ARGS_T>
decltype(auto) make_collection(FIRST_T &&first, SECOND_T && second,
                               ARGS_T &&...args) {
    return std::make_pair(std::forward<FIRST_T>(first),  make_collection(std::forward<SECOND_T>(second),
                                                                         std::forward<ARGS_T>(args)...));
}

template <typename FIRST_T, typename SECOND_T>
decltype(auto) make_collection(FIRST_T &&first, SECOND_T && second) {
    return std::make_pair(std::forward<FIRST_T>(first), std::forward<SECOND_T>(second));
}

template <typename FIRST_T>
decltype(auto) make_collection(FIRST_T &&first) {
    return first;
}

#else // MQTT_CLANG_STL_WORKAROUND

template <typename... ARGS_T>
decltype(auto) make_collection(ARGS_T &&... args) {
    return std::make_tuple(std::forward<ARGS_T>(args)...);
}

#endif //MQTT_CLANG_STL_WORKAROUND

}
#endif // MQTT_MAKE_COLLECTION
