#if !defined(MQTT_DEPRECATED_MSG_HPP)
#define MQTT_DEPRECATED_MSG_HPP

#define MQTT_DEPRECATED_MSG_PUB_STR R"(Use
publish(
    std::string topic_name,
    std::string contents,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_PUB_AS_BUF R"(Use
publish(
    as::const_buffer topic_name,
    as::const_buffer contents,
    any life_keeper,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_PUB_PID_STR R"(Use
publish(
    packet_id_t packet_id,
    std::string topic_name,
    std::string contents,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_PUB_PID_AS_BUF R"(Use
publish(
    packet_id_t packet_id,
    as::const_buffer topic_name,
    as::const_buffer contents,
    any life_keeper,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_PUB_STR R"(Use
acquired_publish(
    packet_id_t packet_id,
    std::string topic_name,
    std::string contents,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_PUB_AS_BUF R"(Use
acquired_publish(
    packet_id_t packet_id,
    as::const_buffer topic_name,
    as::const_buffer contents,
    any life_keeper,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_PUB_STR R"(Use
async_publish(
    std::string topic_name,
    std::string contents,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PROPS_STR R"(Use
async_publish(
    std::string topic_name,
    std::string contents,
    std::uint8_t qos,
    bool retain,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_PUB_AS_BUF R"(Use
async_publish(
    as::const_buffer topic_name,
    as::const_buffer contents,
    any life_keeper,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PROPS_AS_BUF R"(Use
async_publish(
    as::const_buffer topic_name,
    as::const_buffer contents,
    any life_keeper,
    std::uint8_t qos,
    bool retain,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PID_STR R"(Use
async_publish(
    packet_id_t packet_id,
    std::string topic_name,
    std::string contents,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PID_AS_BUF R"(Use
async_publish(
    packet_id_t packet_id,
    as::const_buffer topic_name,
    as::const_buffer contents,
    any life_keeper,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PID_PROPS_STR R"(Use
async_publish(
    packet_id_t packet_id,
    std::string topic_name,
    std::string contents,
    std::uint8_t qos,
    bool retain,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_PUB_PID_PROPS_AS_BUF R"(Use
async_publish(
    packet_id_t packet_id,
    as::const_buffer topic_name,
    as::const_buffer contents,
    any life_keeper,
    std::uint8_t qos,
    bool retain,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)"

#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_PUB_STR R"(Use
acquired_async_publish(
    packet_id_t packet_id,
    std::string topic_name,
    std::string contents,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_PUB_AS_BUF R"(Use
acquired_async_publish(
    packet_id_t packet_id,
    as::const_buffer topic_name,
    as::const_buffer contents,
    any life_keeper,
    std::uint8_t qos = qos::at_most_once,
    bool retain = false,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_PUB_PROPS_STR R"(Use
acquired_async_publish(
    packet_id_t packet_id,
    std::string topic_name,
    std::string contents,
    std::uint8_t qos,
    bool retain,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_PUB_PROPS_AS_BUF R"(Use
acquired_async_publish(
    packet_id_t packet_id,
    as::const_buffer topic_name,
    as::const_buffer contents,
    any life_keeper,
    std::uint8_t qos,
    bool retain,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)"



#define MQTT_DEPRECATED_MSG_SUB_STR R"(Use
subscribe(
    std::vector<std::tuple<string_view, std::uint8_t>> params,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_SUB_AS_BUF R"(Use
subscribe(
    std::vector<std::tuple<buffer, std::uint8_t>> params,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_UNSUB_STR R"(Use
unsubscribe(
    std::vector<string_view> params,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_UNSUB_AS_BUF R"(Use
unsubscribe(
    std::vector<as::const_buffer> params,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_SUB_PID R"(Use
subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<string_view, std::uint8_t>> params,
    std::vector<v5::property_variant> props = {}
)
or
subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<buffer, std::uint8_t>> params,
    std::vector<v5::property_variant> props = {}
)

)"

#define MQTT_DEPRECATED_MSG_UNSUB_PID R"(Use
unsubscribe(
    packet_id_t packet_id,
    std::vector<string_view> params,
    std::vector<v5::property_variant> props = {}
)
or
unsubscribe(
    packet_id_t packet_id,
    std::vector<as::const_buffer> params,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_SUB R"(Use
acqired_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<string_view, std::uint8_t>> params,
    std::vector<v5::property_variant> props = {}
)
or
acqired_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<buffer, std::uint8_t>> params,
    std::vector<v5::property_variant> props = {}
)

)"

#define MQTT_DEPRECATED_MSG_ACQ_UNSUB R"(Use
acqired_unsubscribe(
    packet_id_t packet_id,
    std::vector<string_view> params,
    std::vector<v5::property_variant> props = {}
)
or
acqired_unsubscribe(
    packet_id_t packet_id,
    std::vector<as::const_buffer> params,
    std::vector<v5::property_variant> props = {}
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_SUB_STR R"(Use
async_subscribe(
    std::vector<std::tuple<std::string, std::uint8_t>> const& params,
    async_handler_t func = async_handler_t()
)
or
async_subscribe(
    std::vector<std::tuple<std::string, std::uint8_t>> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_SUB_AS_BUF R"(Use
async_subscribe(
    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
    async_handler_t func = async_handler_t()
)
or
async_subscribe(
    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_UNSUB_STR R"(Use
async_unsubscribe(
    std::vector<std::string> const& params,
    async_handler_t func = async_handler_t()
)
or
async_unsubscribe(
    std::vector<std::string> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_UNSUB_AS_BUF R"(Use
async_unsubscribe(
    std::vector<as::const_buffer> const& params,
    async_handler_t func = async_handler_t()
)
or
async_unsubscribe(
    std::vector<as::const_buffer> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"


#define MQTT_DEPRECATED_MSG_ASYNC_SUB_PID_STR R"(Use
async_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<std::string, std::uint8_t>> const& params,
    async_handler_t func = async_handler_t()
)
or
async_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<std::string, std::uint8_t>> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_SUB_PID_AS_BUF R"(Use
async_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
    async_handler_t func = async_handler_t()
)
or
async_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_UNSUB_PID_STR R"(Use
async_unsubscribe(
    packet_id_t packet_id,
    std::vector<std::string> const& params,
    async_handler_t func = async_handler_t()
)
or
async_unsubscribe(
    packet_id_t packet_id,
    std::vector<std::string> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ASYNC_UNSUB_PID_AS_BUF R"(Use
async_unsubscribe(
    packet_id_t packet_id,
    std::vector<as::const_buffer> const& params,
    async_handler_t func = async_handler_t()
)
or
async_unsubscribe(
    packet_id_t packet_id,
    std::vector<as::const_buffer> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_SUB_STR R"(Use
acquired_async_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<std::string, std::uint8_t>> const& params,
    async_handler_t func = async_handler_t()
)
or
acquired_async_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<std::string, std::uint8_t>> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_SUB_AS_BUF R"(Use
acquired_async_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
    async_handler_t func = async_handler_t()
)
or
acquired_async_subscribe(
    packet_id_t packet_id,
    std::vector<std::tuple<as::const_buffer, std::uint8_t>> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_UNSUB_STR R"(Use
acquired_async_unsubscribe(
    packet_id_t packet_id,
    std::vector<std::string> const& params,
    async_handler_t func = async_handler_t()
)
or
acquired_async_unsubscribe(
    packet_id_t packet_id,
    std::vector<std::string> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#define MQTT_DEPRECATED_MSG_ACQ_ASYNC_UNSUB_AS_BUF R"(Use
acquired_async_unsubscribe(
    packet_id_t packet_id,
    std::vector<as::const_buffer> const& params,
    async_handler_t func = async_handler_t()
)
or
acquired_async_unsubscribe(
    packet_id_t packet_id,
    std::vector<as::const_buffer> const& params,
    std::vector<v5::property_variant> props,
    async_handler_t func = async_handler_t()
)
)"

#endif // MQTT_DEPRECATED_MSG_HPP
