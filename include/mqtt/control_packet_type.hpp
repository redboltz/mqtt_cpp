// Copyright Takatoshi Kondo 2015
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(CONTROL_PACKET_TYPE_HPP)
#define CONTROL_PACKET_TYPE_HPP

namespace mqtt {

enum class control_packet_type {
 // reserved    =  0,
    connect     =  1,
    connack     =  2,
    publish     =  3,
    puback      =  4,
    pubrec      =  5,
    pubrel      =  6,
    pubcomp     =  7,
    subscribe   =  8,
    suback      =  9,
    unsubscribe = 10,
    unsuback    = 11,
    pingreq     = 12,
    pingresp    = 13,
    disconnect  = 14,
 // reserved    = 15
};

} // namespace mqtt

#endif // CONTROL_PACKET_TYPE_HPP
