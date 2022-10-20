# Copyright Takatoshi Kondo 2020
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

CMAKE_MINIMUM_REQUIRED (VERSION 3.8.2)

OPTION(MQTT_TEST_1 "Build test part1" ON)
OPTION(MQTT_TEST_2 "Build test part2" ON)
OPTION(MQTT_TEST_3 "Build test part3" ON)
OPTION(MQTT_TEST_4 "Build test part4" ON)
OPTION(MQTT_TEST_5 "Build test part5" ON)
OPTION(MQTT_TEST_6 "Build test part6" ON)
OPTION(MQTT_TEST_7 "Build test part7" ON)
OPTION(MQTT_TEST_8 "Build test part8" ON)

ADD_SUBDIRECTORY (system)
ADD_SUBDIRECTORY (unit)
