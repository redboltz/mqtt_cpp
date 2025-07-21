load("//bazel/rules:cc.bzl", "cc_binary", "cc_library")

cc_library(
    name = "mqtt_cpp",
    hdrs = glob(["include/**/*.hpp",]),
    includes = ["include"],
    defines = ["MQTT_USE_TLS",
               "MQTT_USE_STATIC_BOOST",
               "MQTT_USE_STATIC_OPENSSL",
               "MQTT_STD_VARIANT",
               "MQTT_STD_OPTIONAL",
               "MQTT_STD_STRING_VIEW",
               "MQTT_STD_ANY",
               "MQTT_STD_SHARED_PTR_ARRAY",
               ],
    deps = [
        "@boost//:boost_asio_ssl",
        "@boost//:boost_log",
        "@boost//:system",
    ],
    visibility = ["//visibility:public"],
)