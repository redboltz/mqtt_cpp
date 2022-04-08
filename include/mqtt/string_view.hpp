// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_STRING_VIEW_HPP)
#define MQTT_STRING_VIEW_HPP

#include <mqtt/namespace.hpp>

#ifdef MQTT_STD_STRING_VIEW

#include <string_view>

namespace MQTT_NS {

using std::string_view;

using std::basic_string_view;

namespace detail {
    
template<class T>
T* to_address(T* p) noexcept
{
    return p;
}
 
template<class T>
auto to_address(const T& p) noexcept
{
    return detail::to_address(p.operator->());
}

} // namespace detail

// Make string_view from a potentially fancy iterator and a size. This is a common mistake when 
// using compilers like GCC which by default define std::string_view::iterator as char*.
template<class Begin>
string_view make_string_view(Begin begin, string_view::size_type size) {
    return string_view(detail::to_address(begin), size);
}

} // namespace MQTT_NS

#else  // MQTT_STD_STRING_VIEW

#include <boost/version.hpp>

#if !defined(MQTT_NO_BOOST_STRING_VIEW)

#if BOOST_VERSION >= 106100

#define MQTT_NO_BOOST_STRING_VIEW 0

#include <boost/utility/string_view.hpp>
#include <boost/container_hash/hash_fwd.hpp>

namespace MQTT_NS {

using string_view = boost::string_view;

template<class CharT, class Traits = std::char_traits<CharT> >
using basic_string_view = boost::basic_string_view<CharT, Traits>;

template<class Begin>
string_view make_string_view(Begin&& begin, string_view::size_type size) {
    return string_view(std::forward<Begin>(begin), size);
}

} // namespace MQTT_NS

#if BOOST_VERSION < 106900
namespace boost {
template <class charT, class traits>
std::size_t hash_value(basic_string_view<charT, traits> s) {
    return hash_range(s.begin(), s.end());
}
}

#endif // BOOST_VERSION < 106900

#else  // BOOST_VERSION >= 106100

#define MQTT_NO_BOOST_STRING_VIEW 1

#include <boost/utility/string_ref.hpp>

namespace MQTT_NS {

using string_view = boost::string_ref;

template<class CharT, class Traits = std::char_traits<CharT> >
using basic_string_view = boost::basic_string_ref<CharT, Traits>;

template<class Begin>
string_view make_string_view(Begin&& begin, string_view::size_type size) {
    return string_view(std::forward<Begin>(begin), size);
}

} // namespace MQTT_NS

#endif // BOOST_VERSION >= 106100


#endif // !defined(MQTT_NO_BOOST_STRING_VIEW)

#endif // !defined(MQTT_STD_STRING_VIEW)

#endif // MQTT_STRING_VIEW_HPP
