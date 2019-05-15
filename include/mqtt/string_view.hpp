// Copyright Takatoshi Kondo 2016
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_STRING_VIEW_HPP)
#define MQTT_STRING_VIEW_HPP

#ifdef MQTT_STD_STRING_VIEW

#include <string_view>

namespace mqtt {

using std::string_view;

using std::basic_string_view;

} // namespace mqtt

#else  // MQTT_STD_STRING_VIEW

#include <boost/version.hpp>

#if !defined(MQTT_NO_BOOST_STRING_VIEW)

#if BOOST_VERSION >= 106100

#define MQTT_NO_BOOST_STRING_VIEW 0

#include <boost/utility/string_view.hpp>

namespace mqtt {

using string_view = boost::string_view;

template<class CharT, class Traits = std::char_traits<CharT> >
using basic_string_view = boost::basic_string_view<CharT, Traits>;

} // namespace mqtt

#else  // BOOST_VERSION >= 106100

#define MQTT_NO_BOOST_STRING_VIEW 1

#include <boost/utility/string_ref.hpp>

namespace mqtt {

using string_view = boost::string_ref;

template<class CharT, class Traits = std::char_traits<CharT> >
using basic_string_view = boost::basic_string_ref<CharT, Traits>;

} // namespace mqtt

#endif // BOOST_VERSION >= 106100

#endif // !defined(MQTT_NO_BOOST_STRING_VIEW)

#endif // !defined(MQTT_STD_STRING_VIEW)

#endif // MQTT_STRING_VIEW_HPP
