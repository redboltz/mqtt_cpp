#if !defined(MQTT_DEPRECATED_MSG_HPP)
#define MQTT_DEPRECATED_MSG_HPP

#define MQTT_DEPRECATED_MSG_SUBACK "Use\n"   \
"suback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<suback_reason_code, v5::suback_reason_code> reason,\n" \
"    v5::properties props = {}\n" \
")\n" \
"or\n" \
"suback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<std::vector<suback_reason_code>, std::vector<v5::suback_reason_code>> reasons,\n" \
"    v5::properties props = {}\n" \
")\n"

#define MQTT_DEPRECATED_MSG_UNSUBACK "Use\n"   \
"unsuback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<unsuback_reason_code, v5::unsuback_reason_code> reason,\n" \
"    v5::properties props = {}\n" \
")\n" \
"or\n" \
"unsuback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<std::vector<unsuback_reason_code>, std::vector<v5::unsuback_reason_code>> reasons,\n" \
"    v5::properties props = {}\n" \
")\n"

#define MQTT_DEPRECATED_MSG_ASYNC_SUBACK "Use\n"   \
"async_suback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<suback_reason_code, v5::suback_reason_code> reason,\n" \
"    any session_life_keeper = any()\n" \
")\n" \
"or\n" \
"async_suback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<suback_reason_code, v5::suback_reason_code> reason,\n" \
"    v5::properties props,\n" \
"    any session_life_keeper = any()\n" \
")\n" \
"or\n" \
"async_suback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<std::vector<suback_reason_code>, std::vector<v5::suback_reason_code>> reasons,\n" \
"    any session_life_keeper = any()\n" \
")\n" \
"or\n" \
"async_suback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<std::vector<suback_reason_code>, std::vector<v5::suback_reason_code>> reasons,\n" \
"    v5::properties props,\n" \
"    any session_life_keeper = any()\n" \
")\n"

#define MQTT_DEPRECATED_MSG_ASYNC_UNSUBACK "Use\n"   \
"async_unsuback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<unsuback_reason_code, v5::unsuback_reason_code> reason,\n" \
"    any session_life_keeper = any()\n" \
")\n" \
"or\n" \
"async_unsuback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<unsuback_reason_code, v5::unsuback_reason_code> reason,\n" \
"    v5::properties props,\n" \
"    any session_life_keeper = any()\n" \
")\n" \
"or\n" \
"async_unsuback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<std::vector<unsuback_reason_code>, std::vector<v5::unsuback_reason_code>> reasons,\n" \
"    any session_life_keeper = any()\n" \
")\n" \
"or\n" \
"async_unsuback(\n" \
"    packet_id_t packet_id,\n" \
"    variant<std::vector<unsuback_reason_code>, std::vector<v5::unsuback_reason_code>> reasons,\n" \
"    v5::properties props,\n" \
"    any session_life_keeper = any()\n" \
")\n"



#endif // MQTT_DEPRECATED_MSG_HPP
