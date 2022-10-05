// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_CONFIG_HPP)
#define MQTT_CONFIG_HPP

#if !defined(MQTT_STD_VARIANT)

// Use boost variant

// user intentionally defined BOOST_MPL_LIMIT_LIST_SIZE but size is too small
// NOTE: if BOOST_MPL_LIMIT_LIST_SIZE is not defined, the value is evaluate as 0.
#if defined(BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS) && BOOST_MPL_LIMIT_LIST_SIZE < 40

#error BOOST_MPL_LIMIT_LIST_SIZE need to greator or equal to 40

#else  // defined(BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS) && BOOST_MPL_LIMIT_LIST_SIZE < 40

// user doesn't define BOOST_MPL_LIMIT_LIST_SIZE intentionally
// but the defult value could be defined

#undef BOOST_MPL_LIMIT_LIST_SIZE
#define BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
#define BOOST_MPL_LIMIT_LIST_SIZE 40

#endif // defined(BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS) && BOOST_MPL_LIMIT_LIST_SIZE < 40


#endif // !defined(MQTT_STD_VARIANT)


// Determine Boost Asio version
#include <boost/asio/version.hpp>

// Make sure standard executors are supported by Boost Asio
#if BOOST_ASIO_VERSION < 101800
#error Boost Asio version 1.18.0 required for no TS-style executors
#endif // BOOST_ASIO_VERSION < 101800

#define BOOST_UUID_FORCE_AUTO_LINK

#endif // MQTT_CONFIG_HPP
