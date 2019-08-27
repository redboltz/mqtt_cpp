#if !defined(MQTT_DEPRECATED_MSG_HPP)
#define MQTT_DEPRECATED_MSG_HPP

#define MQTT_DEPRECATED_MSG_PUB_STR "(Use" \
"publish(\n" \
"    std::string topic_name,\n" \
"    std::string contents,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"

#define MQTT_DEPRECATED_MSG_PUB_AS_BUF "Use\n" \
"publish(\n" \
"    as::const_buffer topic_name,\n" \
"    as::const_buffer contents,\n" \
"    any life_keeper,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_PUB_PID_STR "Use\n" \
"publish(\n" \
"    packet_id_t packet_id,\n" \
"    std::string topic_name,\n" \
"    std::string contents,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_PUB_PID_AS_BUF "Use\n" \
"publish(\n" \
"    packet_id_t packet_id,\n" \
"    as::const_buffer topic_name,\n" \
"    as::const_buffer contents,\n" \
"    any life_keeper,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_PUB_STR "Use\n" \
"acquired_publish(\n" \
"    packet_id_t packet_id,\n" \
"    std::string topic_name,\n" \
"    std::string contents,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_PUB_AS_BUF "Use\n" \
"acquired_publish(\n" \
"    packet_id_t packet_id,\n" \
"    as::const_buffer topic_name,\n" \
"    as::const_buffer contents,\n" \
"    any life_keeper,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_PUB_STR "Use\n" \
"async_publish(\n" \
"    std::string topic_name,\n" \
"    std::string contents,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PROPS_STR "Use\n" \
"async_publish(\n" \
"    std::string topic_name,\n" \
"    std::string contents,\n" \
"    std::uint8_t qos,\n" \
"    bool retain,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_PUB_AS_BUF "Use\n" \
"async_publish(\n" \
"    as::const_buffer topic_name,\n" \
"    as::const_buffer contents,\n" \
"    any life_keeper,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PROPS_AS_BUF "Use\n" \
"async_publish(\n" \
"    as::const_buffer topic_name,\n" \
"    as::const_buffer contents,\n" \
"    any life_keeper,\n" \
"    std::uint8_t qos,\n" \
"    bool retain,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PID_STR "Use\n" \
"async_publish(\n" \
"    packet_id_t packet_id,\n" \
"    std::string topic_name,\n" \
"    std::string contents,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PID_AS_BUF "Use\n" \
"async_publish(\n" \
"    packet_id_t packet_id,\n" \
"    as::const_buffer topic_name,\n" \
"    as::const_buffer contents,\n" \
"    any life_keeper,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PID_PROPS_STR "Use\n" \
"async_publish(\n" \
"    packet_id_t packet_id,\n" \
"    std::string topic_name,\n" \
"    std::string contents,\n" \
"    std::uint8_t qos,\n" \
"    bool retain,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PID_PROPS_AS_BUF "Use\n" \
"async_publish(\n" \
"    packet_id_t packet_id,\n" \
"    as::const_buffer topic_name,\n" \
"    as::const_buffer contents,\n" \
"    any life_keeper,\n" \
"    std::uint8_t qos,\n" \
"    bool retain,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"

#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_PUB_STR "Use\n" \
"acquired_async_publish(\n" \
"    packet_id_t packet_id,\n" \
"    std::string topic_name,\n" \
"    std::string contents,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_PUB_AS_BUF "Use\n" \
"acquired_async_publish(\n" \
"    packet_id_t packet_id,\n" \
"    as::const_buffer topic_name,\n" \
"    as::const_buffer contents,\n" \
"    any life_keeper,\n" \
"    std::uint8_t qos = qos::at_most_once,\n" \
"    bool retain = false,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_PUB_PROPS_STR "Use\n" \
"acquired_async_publish(\n" \
"    packet_id_t packet_id,\n" \
"    std::string topic_name,\n" \
"    std::string contents,\n" \
"    std::uint8_t qos,\n" \
"    bool retain,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_PUB_PROPS_AS_BUF "Use\n" \
"acquired_async_publish(\n" \
"    packet_id_t packet_id,\n" \
"    as::const_buffer topic_name,\n" \
"    as::const_buffer contents,\n" \
"    any life_keeper,\n" \
"    std::uint8_t qos,\n" \
"    bool retain,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"



#define MQTT_DEPRECATED_MSG_SUB_STR "Use\n" \
"subscribe(\n" \
"    std::vector<std::tuple<string_view, std::uint8_t>> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_SUB_AS_BUF "Use\n" \
"subscribe(\n" \
"    std::vector<std::tuple<buffer, std::uint8_t>> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_UNSUB_STR "Use\n" \
"unsubscribe(\n" \
"    std::vector<string_view> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_UNSUB_AS_BUF "Use\n" \
"unsubscribe(\n" \
"    std::vector<as::const_buffer> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_SUB_PID "Use\n" \
"subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<string_view, std::uint8_t>> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n" \
"or\n" \
"subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<buffer, std::uint8_t>> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"



#define MQTT_DEPRECATED_MSG_UNSUB_PID "Use\n" \
"unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<string_view> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n" \
"or\n" \
"unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<as::const_buffer> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_SUB "Use\n" \
"acqired_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<string_view, std::uint8_t>> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n" \
"or\n" \
"acqired_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<buffer, std::uint8_t>> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"



#define MQTT_DEPRECATED_MSG_ACQ_UNSUB "Use\n" \
"acqired_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<string_view> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n" \
"or\n" \
"acqired_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<as::const_buffer> params,\n" \
"    std::vector<v5::property_variant> props = {}\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_SUB_STR "Use\n" \
"async_subscribe(\n" \
"    std::vector<std::tuple<std::string, std::uint8_t>> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"async_subscribe(\n" \
"    std::vector<std::tuple<std::string, std::uint8_t>> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_SUB_AS_BUF "Use\n" \
"async_subscribe(\n" \
"    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"async_subscribe(\n" \
"    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_UNSUB_STR "Use\n" \
"async_unsubscribe(\n" \
"    std::vector<std::string> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"async_unsubscribe(\n" \
"    std::vector<std::string> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_UNSUB_AS_BUF "Use\n" \
"async_unsubscribe(\n" \
"    std::vector<as::const_buffer> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"async_unsubscribe(\n" \
"    std::vector<as::const_buffer> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"



#define MQTT_DEPRECATED_MSG_ASYNC_SUB_PID_STR "Use\n" \
"async_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<std::string, std::uint8_t>> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"async_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<std::string, std::uint8_t>> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_SUB_PID_AS_BUF "Use\n" \
"async_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"async_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_UNSUB_PID_STR "Use\n" \
"async_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::string> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"async_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::string> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ASYNC_UNSUB_PID_AS_BUF "Use\n" \
"async_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<as::const_buffer> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"async_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<as::const_buffer> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_SUB_STR "Use\n" \
"acquired_async_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<std::string, std::uint8_t>> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"acquired_async_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<std::string, std::uint8_t>> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_SUB_AS_BUF "Use\n" \
"acquired_async_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"acquired_async_subscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_UNSUB_STR "Use\n" \
"acquired_async_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::string> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"acquired_async_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<std::string> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_UNSUB_AS_BUF "Use\n" \
"acquired_async_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<as::const_buffer> const& params,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n" \
"or\n" \
"acquired_async_unsubscribe(\n" \
"    packet_id_t packet_id,\n" \
"    std::vector<as::const_buffer> const& params,\n" \
"    std::vector<v5::property_variant> props,\n" \
"    async_handler_t func = async_handler_t()\n" \
")\n"


#endif // MQTT_DEPRECATED_MSG_HPP
