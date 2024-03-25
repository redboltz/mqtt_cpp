## 13.2.1
* Fixed async socket close blocking problem. (#977)


## 13.2.0
* Added locks for timers. (#973)
* Added logs. (#971)
* Added pre-opened socket support for client. (#970)
* Added example for TLS-SNI. (#961)
* Removed old TS code. (#952)
* Fixed invalid timing async_shutdown handler call. (#950)
* Improved cmake settings. (#948)

## 13.1.0
* Added clear username and password functionality. (#944)
* Refined utility tools. (#941, #942)
* Refined log disabling. (#940)
* Fixed async_shutdown callback twice called problem. (#938)
* Refined error handlling. (#937)

## 13.0.0
* <<<< breaking change >>>> Minimum boost requirement is 1.74.0. (#934)
* Modulized store. (#933)
* Improved compile times and memory consumption. (#931)
* Improved MQTT_STD_XXX behavior. (#929)
* Improved build system. (#927)

## 12.1.0
* Improved CI (#930)
* Improved MQTT_STD_XXX support (#929)
* Removed boost.log include when MQTT_USE_LOG is off (#927)
* Removed async_force_disconnect() call from session_state (#925)
* Added authentication/authorization support (#896, #921, #923)
* Added running_in_this_thread() wrapper (#918)
* Fixed client_id setting bug (#917)
* Fixed resend_pubrel_ registration logic (#914)
* Fixed example code topic comparison (#913)
* Added a CLI(Command Line Interface) mqtt client tool (#911, #920)
* Added setup_socket() on client constructor (#909)
* Added defer wrapper (#908)
* Added dispatch wrapper (#907)
* Added comparison operators for properties (#906)
* Added missing dependencies in Package Configuration File (#903)
* Separated connect_handler functions (#895)
* Refine response_topic life management (#894)
* Made uuid gen to thread_local (#893)
* Supported request_response_information and response_topic (#892)
* Implemented client_id assignment when client send empty one (#891)
* Refined binary buffer operation (#890)
* Fixed correlation_data property (#889)
* Moved start_session() call after all handlers se. (#881)
* Added set options print for broker (#880)
* Added mutex for `retains_` (#879)
* Added a performance meajure tool `bench` (#878, #882, #883, #884, #885, #886, #887, #898, #899, #900, #905)
* Fixed broker side qos2 exactly once logic (#874)
* Added publish handled QoS2 packet id getter and restore (#873)
## 12.0.0
* Improved exampless (#870)
* Fixed example broker's invalid sync connack call (#869)
* Added no CONNACK sending mode to the example broker (#869)
* Added posting code to acceptors (#867)
* Added io_context and threads setting options for example broker (#866, #871, #872)
* Fixed moved from object access (#865)
* <<<< breaking change >>>> Removed ioc_con() and ioc_accept() (#866)
* <<<< breaking change >>>> Unified automatic async/sync choosing flag. (#857, #862)
  endppoint constructor 4th parameter `async_operation` is used.
  set_async_operation() can overwrite the async_operation.
  It should be called before sending any packets.
* Implemented clean shutdown (WS TLS TCP) (#849, #860, #863, #864)
* Improved build system (#859)
* Added DISCONNECT with session taken over reason code (#847)
* Improved tests (#845, #846, #852)
* Fixed invalid maximum_qos (2) sending (#844)
* Added multi thread support for example broker (#842, #855, #866)
* Replaced example broker's API call from sync to async (#842)
* Replaced use_certificate_file() with use_certificate_chain_file() to support both server cert and server - intermediate CA cert (#841)
* <<<< breaking change >>>> Added async_force_disconnect(). force_disconnect() is removed from async_client (#840, #868)
* Added will delay interval support (#839)
* Fixed async_handler_t calling timing (#836)
* Added BOOST_ASIO_NO_TS_EXECUTORS support (#830)
* Fixed async_unsuback() argument mismatch (#828)
* Added receive maximum support(#825, #835)
* Fixed QoS2 packet handling problem (#824)
* Refined async test mechanism. MQTT_ORDERED() macro is introcuded (#822, #823)
* Added maximum packet size support (#821, #826, #834))

## 11.1.0
* Added Topic Alias Maximum setting functionality. (#816, #817, #818)

## 11.0.0
* Added examples. (#812)
* Added automatic topic alias management. See https://github.com/redboltz/mqtt_cpp/wiki/automatic-topic-alias-management (#812)
* <<<< breaking change >>>> Replaced boost type erasure with virtual functions with inline. (#810)
## 10.0.0
* Fixed namespace qualifing. (#808)
* Added host port setter. (#802)
* Improved logger performance. (#798, #799)
* Improved CI. (#796, #801)
* Applied boost asio stackless coroutine for parsing process. (#794)
* Fxied constexpr. (#793)
* Improved tests. (#789)
* Added lifetime query for the buffer. (#787)
* Refined examples. (#785)
* Fixed topic alias. (#783)
* Added certificate update support for the broker. (#781)
* Improved packet_id allocation. (#778)
* Removed copy. (#774)
* Added command line options for the broker. (#772)
* Fixed the broker error handling. (#769)
* Improved session expiry process. (#767)
* Refined error checking. (#766, #770)
* Fixed invalid assertion checking. (#761)
* Added packet id exhausted queuing support for the broker. (#757)
* Fixed the broker crush. (#754)
* <<<< breaking change >>>> Added shared subscription support. (#752, #759, #764, #765)
* Fixed invalid iterator operation on the broker. (#750)
* Removed unused files. (#748)
* Re-organized the test tree. (#747)

## 9.0.0
* Added Websocket sub-ptorocol. (#735)
* Added BOOST_ASIO_NO_DEPRECATED to CI. (#734, #745)
* Added test timeout. (#733)
* Added Message Expiry Interval support to test_broker. (#731, #732, #736)
  * NOTE: client side is not supported yet.
* Added inflight message resending functionality for test_broker. (#730)
  * NOTE: client side already have the functionality.
* Added payload_as_buffer() for publish message. (#727)
* <<<< breaking change >>>> Supported scattered payload publish. (#723, #724)
  * NOTE: The return type of payload() has been changed. Ues payload_as_buffer().
* Impropve examples. (#722)
* Fixed Boost.Test include file. (#721)
* Improved CI. (#718)
* Fixed session expiry. (#700)
* Added Subscription Identifier support. (#683)
* Added NL(No Local) support. (#682)
* Added tests. (#680)
* Added subscription update. (#678, #679)
* Supported alternative TLS library. (#673)
* Added wildcard support to test_broker. (#671, #674, #681, #685, #687, #688, #689, #691, #693, #694, #695, #696, #698, #699, #707, #713, #715, #717, #742)
* Fixed compile error on Boost 1.74.0. (#670)
* Fixed namespace qualification. (#667, #668, #725, #726)
* Fixed warnings. (#665, #677)
* Improved client connect function. (#664)
* Added Topic Alias support. (#660, #666)
* Added strand_wrap for timeout code. (#659)
* <<<< breaking change >>>> Fixed error code. (#658)
* Added Session Expiry Interval functionality. (#657)
* Added MQTT_USE_LOG to enable logging. (#656)
* Improved output stream operator. (#655)
* Fixed user name and password empty case handling. (#654)

## 8.0.0
* Added PINGRESP timeout and made internal force_disconnect() calling in the strand. (#649)
* Added async_client example. (#647)
* Made async_connect actually async. (#643)
* Added missing packet types to switch case. (#642)
* Enabled cmake target configuration. (#632)
* Made mqtt::shared_any force_move the constructor parameter into the member variable. (#629)
* Added io_context getter for servers. (#627)
* Tracked total bytes sent and received on socket. (#625, #641)
* <<<< breaking change >>>> Replaced use of boost::asio::deadline_timer with boost::asio::steady_timer. (#621, #626)
* Added getter for `client_id_`. (#619)
* Use the `-fsyntax-only` flag instead of `--preprocess` for header checking (#617)
* Added inflight subscribe and unsubscribe packet_id clean up mechanism. (#612)
* Added protocol error processing if control packets are received before connect/connack is received. (#611)
* Fixed uninitialized pubrel reason code. (#607)
* Fixed inefficient packet on puback/pubrec/pubrel/pubcomp. (#604)
* Added getters for publish and pubrel message. (#603)
* <<<< breaking change >>>> Fixed message version of for_each_store. (#602)
* Skipped properties phase if remaining length is 0. (#600)
* Updated v5_message.hpp pubrel fixed header flags. (#599)
* <<<< breaking change >>>> Updated minimum boost requirement to 1.67.0. (#595)
* Use std::move instead of std::forward in null_strand. (#594)
* Removed boost::static_visitor inheritance. (#589)
* Refined CI. (#587, #590, #613, #618)
* Added redirect supporting function and an example. (#585)
* Fixed warnings. (#582, #596, #620)
* <<<< breaking change >>>> Removed packet_id from restoring functions. (#581)
* Rifined examples. (#580)
* Added native_handle getting function. (#577)
* Fixed missing base class hander call. (#576)
* Fixed `accumulate` type. (#570)
* Added workaround of libstdc++ std::tuple<std::any> bug. (#568, #569, #571)
* Use mqtt::force_move for test_broker. (#562)
* Added no property overloads for MQTT v3.1.1 users. (#561)
* Fixed missing argument on async_send_pubrel. (#560)
* Migrate CI from travis-ci to github actions. (#553, #556, #564)
* <<<< breaking change >>>> Modified buffer lifetime management strategy to meet Boost.Asio CompletionToken. (#552)
* <<<< breaking change >>>> Added noexcept to event handler virtual functions. User needs to catch the exception in the handlers if an exception is thrown. (#551)
* <<<< breaking change >>>> Introduced publish_options and subscribe_options. Remove *_dup functions. (#548, #579)
* <<<< breaking change >>>> Removed acquired_* family. Now non acquired name version has the overload that takes packet_id parameter. (#547)


## 7.0.1
* Fixed packet_id leak on QoS2 publish. (backported) (#541, #542, #543)

## 7.0.0
* Added explicit destructor to clients. (#481)
* Fixed warnings. (#480)
* Added conversion function from `mqtt::qos` to `suback_return_code` and `v5::suback_reason_code`. (#478)
* Added boost::asio::ssl::context getter. Set `[[deprecated]]` attribute to wrapped function for boost::asio::ssl::context. They will be removed on verion 8.0.0. (#472)
  * mqtt_cpp is now separated from boost::asio::ssl::context detail.
* Fixed boost::type_erasure namespace. (#467)
* Refactoring. (#465)
* Improved receive packet processing. (#462)
* Fixed comments. (#454, #455)
* Added `constexpr`. (#451, #461)
* Use virtual function based polymorphism on endpoint receive packet processing. Existing APIs are preserved as `callable_overlay`. (#444)
  * Thanks to de-virtualization optimization, it is achieved zero-overhead.
* <<<< breaking change >>>> Updated `set_accept_handler()`'s parameter to shared_ptr of the endpoint. (#443)
* <<<< breaking change >>>> Updated `start_session()`'s parameter to `mqtt::any`. It only for life keeping purpose. (#442)
* <<<< breaking change >>>> Remove optional from puback, pubrec, pubrel, and pubcomp. (#441, #470, #476, #477)
* Fixed invalid next packet read on disconnect packet process. (#439)
* <<<< breaking change >>>> Remove optional from the parameter `suback_return_code`(MQTT v3.1.1) of suback handler. (#434)
* <<<< breaking change >>>> Replaced `MQTT_NO_TLS` with `MQTT_USE_TLS` and default value is false. Now, all configure macro has positive meaning. (#428)
* Improved build system. (#425, #459)
* Used old style boost::type_erasure. The new style causes compile error if `-pedantic` flag is set. (#423)
* Introduced include dependency checking build option. (#422)
* <<<< breaking change >>>> Improved parameters of the publish handler. parsing `fixed_header` is no longer required. (#421)
* Replaced boost::asio deprecated API with new ones. (#419, 466)
* Set `[[deprecated]]` attribute to derecated variadic parameter version of suback/unsuback family. They will be removed on verion 8.0.0. (#417)
* Fixed BOOST_VERSION comparison typo. (#413)
* Improved travis-ci. (#412)
* Added boost::multi_index debug flag to travis-ci. (#406)
* <<<< breaking change >>>> Minimum boost required version is 1.66.0. (#403)
* Fixed lack of next read packet bug. (#402)
* Added missing include. (#398)
* <<<< breaking change >>>> Introduced enum class instead of std::uint8_t. (#395, #397, #401, #414, #415, #438)
* Removed invalid WebSocket code from the test. (#394)
* <<<< breaking change >>>> Added subscribe_options class. (#387, #388, #393)
  * subscribe_options can be created from `std::uint8_t` so the impact of existing code is limited.
* <<<< breaking change >>>> Removed deprecated APIs. (#384)

## 6.0.7
* Fixed packet_id leak on QoS2 publish. (backported) (#541, #542, #543)

## 6.0.6
* Fixed server side endpoint lifetime management problem. (#439, #440)

## 6.0.5
* Fixed receive stop problem when unsuback handler is not set. (#408)
* Updated minimum boost requirement to 1.66.0. (#410)
   * NOTE: Boost minimum requirement should be updated at v6.0.0. So this release considered the update as a bug fix, not major version up.

## 6.0.4
* Fixed invalid subscribe qos comparison on receive. (#385)

## 6.0.3
* Fixed sync subscribe/unsubscribe with properties APIs dispatch to deprecated APIs problem. (#383)

## 6.0.2
* Fixed moved from object access problem. (#378)
* Fixed unexpected copy fallback of move operations. (#378)

## 6.0.1
* Fixed compile error of `acquired_async_publish_dup()`. (#376)

## 6.0.0
* Supressed warnings. (#367, #368, #369, #370, #371)
* Set `[[deprecated]]` attribute to derecated APIs. They will be removed on verion 7.0.0. (#364, #372)
* Added `mqtt::buffer` based APIs. (#364)
* Added user defined namespace support. User can define `MQTT_NS` (default `mqtt`). (#360)
* Improved test. (#357)
* Improved test broker. (#353)
* Added allocation fuction for `mqtt::buffer`. (#349)
* Supported old version of boost type_erasure. (#347)
* <<<< breaking change >>>> Adde zero copy receiving functionality. Callback handlers take `mqtt::buffer` parameter instead of `mqtt::string_view`. It can hold the lifetime. (#339, #358, #359, )
* Improved CI. (#333, #346)

## 5.0.1
* Improved CI. (#330)
* Fixed invalid std::any inclusion on mqtt::any. (#328)

## 5.0.0
* Fixed invalid pointer comparison. (#324)
* Fixed user_property value length. (#323)
* Added `store` type alias for each properties. (#319)
* Fixed `BOOST_MPL_LIMIT_LIST_SIZE` comparison. (#318)
* Removed `BOOST_ASSERT` from test code to improve exception message. (#315)
* Code refactoring. (#313)
* Added default parameter to `async_send_pubrel`'s `life_keeper`. (#312)
* Added async version of stored message sending after `CONNACK` received. (#311)
* <<<< breaking change >>>> Removed unnecessery callback invokation. (#309)
* Fixed moved from object access. (#308)
* <<<< breaking change >>>> Added properties and life_keeper parameter to pubrel and async_publrel. (#307)
* Added reuse_address flag to the test broker to avoid "address already in use" on travis-ci. (#306)
* <<<< breaking change >>>> Fixed TCP accepting process. After TCP accepting, do the next accepting process immediately. Due to the fix, Socket is updated to shared_ptr from unique_ptr. (#299)
* <<<< breaking change >>>> Added underlying connect timeout. It can treat well with TCP connected but TLS handshake not sending attack. (#296)
* Added stream output operator for properties. (#293)
* Replaced byte sequence creation macro with function. (#291)
* Fixed static boost support. (#290)
* <<<< breaking change >>>> Fixed property allocation management. (#288)
* Improved MSVC support for test. (#286)
* Added codecov for CI. (#285, #276, #275, #273)
* Added `MQTT_BUILD_EXAMPLES` flag to cmake to shorten CI time. (#284)
* Added non_ref/ref types conversion for properties. (#283)
* Added tests. (#278, #276)
* Improve travis-ci. (#274)
* Migrate from appveyor azure devops build pipelines that supports 10 parallel build. (#270)
* Fixed pingreq async/sync send. (#267)

## 4.0.0
* Added getting ssl keylog callback for debugging. (#256)
* Fixed re-listen logic for servers. Now we can re-listen after accept is failed. (#252, #254)
* Fixed restore lifetime management to avoid accessing memory after destroyed (#251)
* Removed BOOST_ASSERT checking in the case that actually happens if invalid message is received (#249, #250)
* Added MQTT v5 support (#226, #245, #257, #258, #259)
* Added connection overwrite behavior on `test_broker` (#210)
* Added `async_client` and `sync_client` to avoid misusing async and sync APIs. `client` is still remained that can use both sync and async APIs. (#208)
* Added concatenating `const_buffer` functionality on asynchronous packet sending (#207)
* Improved documents (#198, #214, #219, #229, #235)
* Added switching option between `std` and `boost` (#188, #209)
* Improved build mechanisim (#184, #201, #253)
* Code refactoring (#183, #189, #190, #193, #195, #197, #199, #202, #203, #211, #215, #216, #220, #227, #234, #236, #238, #239, #240, #242)
* Support configure time switches for using std::varient or std::optional instead of the boost versions (#182)
* Allow inheriting classes to construct mqtt::client (#181)
* <<<< Breaking change >> Replaced fixed_header type from `char` to `std::uint8_t`. Fixed `-Wconversion` warnings. See https://github.com/redboltz/mqtt_cpp/pull/180/files (#180)

## 3.1.0
* Fixed inefficient passed by value. (#174)
* Fixed unsugscribe message packet id position for continuours buffer. (#167)
* Fixed subscribe message remaining length buffer size. (#165)
* Added broker example. (#164)
* Improved CI. (#161)
* Added 32bit packet id support. It is out of MQTT spec. (#160, #163, #166, #168)
* Added store assertion to detect internal logic error. (#157)
* Fixed life keeper storing timing. (#156, #159)
* Added getter for handlers. (#155)
* Fixed async write callback. (#154)
* Fixed ambiguous function call on C++17 visit call. (#153)
* Improved packet id allocating algorithm. (#152, #162)
* Removed throwing exception when error on close happens. (#151)
* Added re-listen support after listining socket closed. (#150)
* Fixed string_view definition (#146)
* Added string (UTF-8) checking. It is disabled by default. Define MQTT_USE_STR_CHECK to enable it. (#142, #143)
* Improved documents (#141, #131, #172, #173)
* Improved build mechanisim (#140, #147, #148, #170)
* Added custom next mqtt message read handler setting mechanism (#139)

## 3.0.0
* << Breaking change >> mqtt_cpp requires full C++14 feature.
* Added boost::asio::const_buffer interface. User can use it as topic name and contents(payload).
  (#127, #134, #136)
* Improved CI environment. (#129, #130)
* Fixed packet_id leak. (#132)
* Fixed queuing message leak. (#133)
* Added disconnect timeout. (#135)
* Added connect status check function. (#131)

## 2.1.0
* Added packet length checking handler for variable length MQTT packet. (#121)
* Fixed doxygen comments. (#119)
* Improved payload memory management. (#118)
* Fixed malformed connack (send by client) problem. (#117)
* Fixed missing packet length check on connect. (#116)
* Improved pingreq sending timing. (#115)

## 2.0.0
* << Breaking change >> Updated close_handler and error_handler invoking rule.
  When the socket is closed,
  if user calls disconnect() and the socket closed by server cleanly, then close_handler is called,
  otherwise error_handler is called.
* Added publish and pubrel message serialize mechanism. (#111)
* Added boost beast support. (#106)
* Added boost 1.66.0 asio support. (#106)

## 1.0.9
* Supported Boost 1.66.0 new asio interface. (#106)
* Updated beast to v124. (#107)
* If Boost version is 1.66.0 or later, use Boost.Beast. (#107)
* Improved tests. (#102, #104)
* Improved assetions. (#105)

## 1.0.8
* Added TLS verifying setup functions. (#100)

## 1.0.7
* Update beast. (#94)
* Fixed WebSocket strand management. (#93, #95)
* Fixed endpoint's move constructor and assingment operator deletion (#92)

## 1.0.6
* Update beast. (#90)
* Fixed qos2 publish handler multiple call guard.(#89)

## 1.0.5
* Renamed to `mqtt_cpp` from `mqtt_client_cpp`. (#88)
* Improved tests. (#87)
* Added MQTT server functionality (#84)
* Supressed warnings. (#82)
* Fixed WebSocket close. (#81)
* Added tls context settings. (#80, #83)
* Fixed force_disconnect. (#79)
* Removed TLS manual shutdown call. (#78)
* Fixed timer management algorithm. (#76)
* Fixed pingreq sending timing. (#75)
* Added bad timming mqtt command receiving guard. (#70)

## 1.0.4
* Added offline publish (Clean Session: false, qos: at_least_once, exactly_once). (#69)
* Prepared for OpenSSL 1.1.0 support. (#68)
* Added explicit packet_id acquieing operation. (#67)
* Used C++14 depended code conditionally. (#62, #63)
* Added WebSocket support. (#61, #66)
* Fixed strand. (#60)
* Added strand to shutdown. (#59)
* Fixed the type of qos on async_send_publish. (#58)
* Added async_disconnect function. (#57)
* Fixed async subscribe/unsubscribe. (#56)
* Improved async operations. (#55)
* Fixed SSL_R_SHORT_READ as normal close, not error. (#54)
* Replaced pair with tuple. (#53)
* Fixed endpoint memory management. (#52)

## 1.0.3
* Fixed force disconnect logic. (#50)
* Added remaining length getting function. (#48)
* Added publish with dup flag function. (#47)
* Added for_each function for stored data. (#44)
* Fixed pubrel re-send algorithm. (#43)
* Added pubrel sending interface. (#41)
* Added sent after callback function. (#39)
* Improved CI environment. (#37)
* Improved documents. (#35, #36)
* Removed generic lambda to meet C++11 (#34)
* Added stored packet clearing function. (#32)
* Fixed QoS2 pubrel sending timing.(#31, #46, #49)
* Fixed remaining length handling logic. (#30)
* Added subscribe and unsubscribe with vector. (#29)
* Added suback with vector. (#28)
* Fixed async subscribe bug. (#26)
* Added connect() with socket. (#25)
* Fixed packet_id accqireing algorithm. (#24, #27)
* Fixed storing QoS1 and 2 packet mechanism. (#23)
* Supported async_write operation. (#21)
* Separated MQTT core from client. Core can be used as server connection. (#20)

## 1.0.2
* Fixed queued message clearing problem. (#18)
* Fixed implicit char conversion problem. (#17)
* Fixed missing remaining length checking. (#16)

## 1.0.1
* Updated README.md version number.

## 1.0.0
* Implemented all mqtt functionarities.
