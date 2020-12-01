#if !defined(MQTT_TLS_ERROR_COMPARISON)

#if defined(SSL_R_SHORT_READ)

#define MQTT_TLS_ERROR_COMPARISON ERR_GET_REASON(error_val) == SSL_R_SHORT_READ;

#else // defined(SSL_R_SHORT_READ)

#define MQTT_TLS_ERROR_COMPARISON ERR_GET_REASON(error_val) == tls::error::stream_truncated;

#endif // defined(SSL_R_SHORT_READ)

#endif // !defined(MQTT_TLS_ERROR_COMPARISON)


        if (   (ec == as::error::eof)
            || (ec == as::error::connection_reset)
            || (ec == as::error::operation_aborted)
#if defined(MQTT_USE_WS)
            || (ec == boost::beast::websocket::error::closed)
#endif // defined(MQTT_USE_WS)
#if defined(MQTT_USE_TLS)
            || (MQTT_TLS_ERROR_COMPARISON)
#endif // defined(MQTT_USE_TLS)
        ) {
        }
