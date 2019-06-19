// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_PROPERTY_HPP)
#define MQTT_PROPERTY_HPP

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include <mqtt/optional.hpp>

#include <boost/asio/buffer.hpp>
#include <boost/container/static_vector.hpp>

#include <mqtt/two_byte_util.hpp>
#include <mqtt/const_buffer_util.hpp>
#include <mqtt/exception.hpp>
#include <mqtt/string_check.hpp>
#include <mqtt/property_id.hpp>
#include <mqtt/four_byte_util.hpp>
#include <mqtt/utf8encoded_strings.hpp>
#include <mqtt/qos.hpp>
#include <mqtt/variable_length.hpp>

namespace mqtt {

namespace as = boost::asio;

namespace v5 {

namespace property {

namespace detail {

enum class ostream_format {
    direct,
    int_cast,
    key_val,
    binary_string,
};

template <std::size_t N>
struct n_bytes_property {
    explicit n_bytes_property(property::id id)
        :id_(id) {}

    template <typename It>
    n_bytes_property(property::id id, It b, It e)
        :id_(id), buf_(b, e) {}

    n_bytes_property(property::id id, boost::container::static_vector<char, N> buf)
        :id_(id), buf_(std::move(buf)) {}

    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(buf_.data(), buf_.size()));
    }

    /**
     * @brief Copy the internal information to the range between b and e
     *        it is for boost asio APIs
     * @param b begin of the range to fill
     * @param e end of the range to fill
     */
    template <typename It>
    void fill(It b, It e) const {
        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());
        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        std::copy(buf_.begin(), buf_.end(), b);
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + buf_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return 2;
    }

    static constexpr ostream_format const of_ = ostream_format::direct;
    property::id id_;
    boost::container::static_vector<char, N> buf_;
};

struct binary_property_ref;

struct binary_property {
    binary_property(property::id id, string_view sv)
        :id_(id),
         length_{num_to_2bytes(static_cast<std::uint16_t>(sv.size()))},
         buf_(sv.begin(), sv.end()) {
             if (sv.size() > 0xffff) throw property_length_error();
         }

    binary_property(binary_property_ref const& v);

    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(length_.data(), length_.size()));
        v.emplace_back(as::buffer(buf_.data(), buf_.size()));
    }

    /**
     * @brief Copy the internal information to the range between b and e
     *        it is for boost asio APIs
     * @param b begin of the range to fill
     * @param e end of the range to fill
     */
    template <typename It>
    void fill(It b, It e) const {
        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());
        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        std::copy(length_.begin(), length_.end(), b);
        b += static_cast<typename It::difference_type>(length_.size());
        std::copy(buf_.begin(), buf_.end(), b);
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + length_.size() + buf_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return 2;
    }

    string_view val() const {
        return buf_;
    }

    static constexpr ostream_format const of_ = ostream_format::direct;
    property::id id_;
    boost::container::static_vector<char, 2> length_;
    std::string buf_;
};

struct binary_property_ref {
    binary_property_ref(property::id id, string_view sv)
        :id_(id),
         length_{num_to_2bytes(static_cast<std::uint16_t>(sv.size()))},
         buf_(sv.data(), sv.size()) {
             if (sv.size() > 0xffff) throw property_length_error();
         }

    binary_property_ref(binary_property const&);
    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(length_.data(), length_.size()));
        v.emplace_back(as::buffer(get_pointer(buf_), get_size(buf_)));
    }

    /**
     * @brief Copy the internal information to the range between b and e
     *        it is for boost asio APIs
     * @param b begin of the range to fill
     * @param e end of the range to fill
     */
    template <typename It>
    void fill(It b, It e) const {
        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());
        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        std::copy(length_.begin(), length_.end(), b);
        b += static_cast<typename It::difference_type>(length_.size());
        std::copy(get_pointer(buf_), get_pointer(buf_) + get_size(buf_), b);
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + length_.size() + get_size(buf_);
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return 3;
    }

    string_view val() const {
        return string_view(get_pointer(buf_), get_size(buf_));
    }

    static constexpr ostream_format const of_ = ostream_format::direct;
    property::id id_;
    boost::container::static_vector<char, 2> length_;
    as::const_buffer buf_;
};

inline binary_property::binary_property(binary_property_ref const& v)
    :id_(v.id_), length_(v.length_), buf_(get_pointer(v.buf_), get_size(v.buf_)) {}

inline binary_property_ref::binary_property_ref(binary_property const& v)
    :id_(v.id_), length_(v.length_), buf_(as::buffer(v.buf_)) {}


struct string_property_ref;

struct string_property : binary_property {
    string_property(property::id id, string_view sv)
        :binary_property(id, sv) {
        auto r = utf8string::validate_contents(sv);
        if (r != utf8string::validation::well_formed) throw utf8string_contents_error(r);
    }
    string_property(string_property_ref const& v);
};

struct string_property_ref : binary_property_ref {
    string_property_ref(property::id id, string_view sv)
        :binary_property_ref(id, sv) {
        auto r = utf8string::validate_contents(sv);
        if (r != utf8string::validation::well_formed) throw utf8string_contents_error(r);
    }
    string_property_ref(string_property const& v);
};

inline string_property::string_property(string_property_ref const& v)
    :binary_property(v) {}

inline string_property_ref::string_property_ref(string_property const& v)
    :binary_property_ref(v) {}

struct variable_property {
    variable_property(property::id id, std::size_t value)
        :id_(id)  {
        variable_push(value_, value);
    }

    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(value_.data(), value_.size()));
    }

    /**
     * @brief Copy the internal information to the range between b and e
     *        it is for boost asio APIs
     * @param b begin of the range to fill
     * @param e end of the range to fill
     */
    template <typename It>
    void fill(It b, It e) const {
        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());
        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        std::copy(value_.begin(), value_.end(), b);
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return 1 + value_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return 2;
    }

    std::size_t val() const {
        return std::get<0>(variable_length(value_));
    }

    static constexpr ostream_format const of_ = ostream_format::direct;
    property::id id_;
    boost::container::static_vector<char, 4> value_;
};

} // namespace detail

class payload_format_indicator : public detail::n_bytes_property<1> {
public:
    using recv = payload_format_indicator;
    enum payload_format {
        binary,
        string
    };

    payload_format_indicator(payload_format fmt = binary)
        : detail::n_bytes_property<1>(id::payload_format_indicator, { fmt == binary ? char(0) : char(1) } ) {}

    template <typename It>
    payload_format_indicator(It b, It e)
        : detail::n_bytes_property<1>(id::payload_format_indicator, b, e) {}

    payload_format val() const {
        return
            [this] {
                if (buf_[0] == 0) return binary;
                else return string;
            }();
    }

    static constexpr detail::ostream_format const of_ = detail::ostream_format::binary_string;
};


class message_expiry_interval : public detail::n_bytes_property<4> {
public:
    using recv = message_expiry_interval;
    message_expiry_interval(std::uint32_t val)
        : detail::n_bytes_property<4>(id::message_expiry_interval, num_to_4bytes(val) ) {}

    template <typename It>
    message_expiry_interval(It b, It e)
        : detail::n_bytes_property<4>(id::message_expiry_interval, b, e) {}

    std::uint32_t val() const {
        return make_uint32_t(buf_.begin(), buf_.end());
    }
};


class content_type;

class content_type_ref : public detail::string_property_ref {
public:
    using recv = content_type_ref;
    content_type_ref(string_view type)
        : detail::string_property_ref(id::content_type, type) {}
    content_type_ref(content_type const& v);
};

class content_type : public detail::string_property {
public:
    using recv = content_type_ref;
    content_type(string_view type)
        : detail::string_property(id::content_type, type) {}
    content_type(content_type_ref const& v);
};

inline content_type_ref::content_type_ref(content_type const& v)
    :detail::string_property_ref(v) {}

inline content_type::content_type(content_type_ref const& v)
    :detail::string_property(v) {}


class response_topic;

class response_topic_ref : public detail::string_property_ref {
public:
    using recv = response_topic_ref;
    response_topic_ref(string_view type)
        : detail::string_property_ref(id::response_topic, type) {}
    response_topic_ref(response_topic const& v);
};

class response_topic : public detail::string_property {
public:
    using recv = response_topic_ref;
    response_topic(string_view type)
        : detail::string_property(id::response_topic, type) {}
    response_topic(response_topic_ref const& v);
};

inline response_topic_ref::response_topic_ref(response_topic const& v)
    :detail::string_property_ref(v) {}

inline response_topic::response_topic(response_topic_ref const& v)
    :detail::string_property(v) {}


class correlation_data;

class correlation_data_ref : public detail::string_property_ref {
public:
    using recv = correlation_data_ref;
    correlation_data_ref(string_view type)
        : detail::string_property_ref(id::correlation_data, type) {}
    correlation_data_ref(correlation_data const& v);
};

class correlation_data : public detail::string_property {
public:
    using recv = correlation_data_ref;
    correlation_data(string_view type)
        : detail::string_property(id::correlation_data, type) {}
    correlation_data(correlation_data_ref const& v);
};

inline correlation_data_ref::correlation_data_ref(correlation_data const& v)
    :detail::string_property_ref(v) {}

inline correlation_data::correlation_data(correlation_data_ref const& v)
    :detail::string_property(v) {}


class subscription_identifier : public detail::variable_property {
public:
    using recv = subscription_identifier;
    subscription_identifier(std::size_t subscription_id)
        : detail::variable_property(id::subscription_identifier, subscription_id) {}
};

class session_expiry_interval : public detail::n_bytes_property<4> {
public:
    using recv = session_expiry_interval;
    session_expiry_interval(std::uint32_t val)
        : detail::n_bytes_property<4>(id::session_expiry_interval, { num_to_4bytes(static_cast<std::uint32_t>(val)) } ) {}

    template <typename It>
    session_expiry_interval(It b, It e)
        : detail::n_bytes_property<4>(id::session_expiry_interval, b, e) {}

    std::uint32_t val() const {
        return make_uint32_t(buf_.begin(), buf_.end());
    }
};


class assigned_client_identifier;

class assigned_client_identifier_ref : public detail::string_property_ref {
public:
    using recv = assigned_client_identifier_ref;
    assigned_client_identifier_ref(string_view type)
        : detail::string_property_ref(id::assigned_client_identifier, type) {}
    assigned_client_identifier_ref(assigned_client_identifier const& v);
};

class assigned_client_identifier : public detail::string_property {
public:
    using recv = assigned_client_identifier_ref;
    assigned_client_identifier(string_view type)
        : detail::string_property(id::assigned_client_identifier, type) {}
    assigned_client_identifier(assigned_client_identifier_ref const& v);
};

inline assigned_client_identifier_ref::assigned_client_identifier_ref(assigned_client_identifier const& v)
    :detail::string_property_ref(v) {}

inline assigned_client_identifier::assigned_client_identifier(assigned_client_identifier_ref const& v)
    :detail::string_property(v) {}


class server_keep_alive : public detail::n_bytes_property<2> {
public:
    using recv = server_keep_alive;
    server_keep_alive(std::uint16_t val)
        : detail::n_bytes_property<2>(id::server_keep_alive, { num_to_2bytes(static_cast<std::uint16_t>(val)) } ) {}

    template <typename It>
    server_keep_alive(It b, It e)
        : detail::n_bytes_property<2>(id::server_keep_alive, b, e) {}

    std::uint16_t val() const {
        return make_uint16_t(buf_.begin(), buf_.end());
    }
};

class authentication_method;

class authentication_method_ref : public detail::string_property_ref {
public:
    using recv = authentication_method_ref;
    authentication_method_ref(string_view type)
        : detail::string_property_ref(id::authentication_method, type) {}
    authentication_method_ref(authentication_method const& v);
};

class authentication_method : public detail::string_property {
public:
    using recv = authentication_method_ref;
    authentication_method(string_view type)
        : detail::string_property(id::authentication_method, type) {}
    authentication_method(authentication_method_ref const& v);
};

inline authentication_method_ref::authentication_method_ref(authentication_method const& v)
    :detail::string_property_ref(v) {}

inline authentication_method::authentication_method(authentication_method_ref const& v)
    :detail::string_property(v) {}


class authentication_data;
class authentication_data_ref : public detail::binary_property_ref {
public:
    using recv = authentication_data_ref;
    authentication_data_ref(string_view type)
        : detail::binary_property_ref(id::authentication_data, type) {}
    authentication_data_ref(authentication_data const& v);
};

class authentication_data : public detail::binary_property {
public:
    using recv = authentication_data_ref;
    authentication_data(string_view type)
        : detail::binary_property(id::authentication_data, type) {}
    authentication_data(authentication_data_ref const& v);
};

inline authentication_data_ref::authentication_data_ref(authentication_data const& v)
    :detail::binary_property_ref(v) {}

inline authentication_data::authentication_data(authentication_data_ref const& v)
    :detail::binary_property(v) {}


class request_problem_information : public detail::n_bytes_property<1> {
public:
    using recv = request_problem_information;
    request_problem_information(bool value)
        : detail::n_bytes_property<1>(id::request_problem_information, { value ? char(1) : char(0) } ) {}

    template <typename It>
    request_problem_information(It b, It e)
        : detail::n_bytes_property<1>(id::request_problem_information, b, e) {}

    bool val() const {
        return buf_[0] == 1;
    }
};


class will_delay_interval : public detail::n_bytes_property<4> {
public:
    using recv = will_delay_interval;
    will_delay_interval(std::uint32_t val)
        : detail::n_bytes_property<4>(id::will_delay_interval, { num_to_4bytes(static_cast<std::uint32_t>(val)) } ) {}

    template <typename It>
    will_delay_interval(It b, It e)
        : detail::n_bytes_property<4>(id::will_delay_interval, b, e) {}

    std::uint32_t val() const {
        return make_uint32_t(buf_.begin(), buf_.end());
    }
};

class request_response_information : public detail::n_bytes_property<1> {
public:
    using recv = request_response_information;
    request_response_information(bool value)
        : detail::n_bytes_property<1>(id::request_response_information, { value ? char(1) : char(0) } ) {}

    template <typename It>
    request_response_information(It b, It e)
        : detail::n_bytes_property<1>(id::request_response_information, b, e) {}

    bool val() const {
        return buf_[0] == 1;
    }
};


class response_information;

class response_information_ref : public detail::string_property_ref {
public:
    using recv = response_information_ref;
    response_information_ref(string_view type)
        : detail::string_property_ref(id::response_information, type) {}
    response_information_ref(response_information const& v);
};

class response_information : public detail::string_property {
public:
    using recv = response_information_ref;
    response_information(string_view type)
        : detail::string_property(id::response_information, type) {}
    response_information(response_information_ref const& v);
};

inline response_information_ref::response_information_ref(response_information const& v)
    :detail::string_property_ref(v) {}

inline response_information::response_information(response_information_ref const& v)
    :detail::string_property(v) {}


class server_reference;

class server_reference_ref : public detail::string_property_ref {
public:
    using recv = server_reference_ref;
    server_reference_ref(string_view type)
        : detail::string_property_ref(id::server_reference, type) {}
    server_reference_ref(server_reference const& v);
};

class server_reference : public detail::string_property {
public:
    using recv = server_reference_ref;
    server_reference(string_view type)
        : detail::string_property(id::server_reference, type) {}
    server_reference(server_reference_ref const& v);
};

inline server_reference_ref::server_reference_ref(server_reference const& v)
    :detail::string_property_ref(v) {}

inline server_reference::server_reference(server_reference_ref const& v)
    :detail::string_property(v) {}


class reason_string;

class reason_string_ref : public detail::string_property_ref {
public:
    using recv = reason_string_ref;
    reason_string_ref(string_view type)
        : detail::string_property_ref(id::reason_string, type) {}
    reason_string_ref(reason_string const& v);
};

class reason_string : public detail::string_property {
public:
    using recv = reason_string_ref;
    reason_string(string_view type)
        : detail::string_property(id::reason_string, type) {}
    reason_string(reason_string_ref const& v);
};

inline reason_string_ref::reason_string_ref(reason_string const& v)
    :detail::string_property_ref(v) {}

inline reason_string::reason_string(reason_string_ref const& v)
    :detail::string_property(v) {}

class receive_maximum : public detail::n_bytes_property<2> {
public:
    using recv = receive_maximum;
    receive_maximum(std::uint16_t val)
        : detail::n_bytes_property<2>(id::receive_maximum, { num_to_2bytes(static_cast<std::uint16_t>(val)) } ) {}

    template <typename It>
    receive_maximum(It b, It e)
        : detail::n_bytes_property<2>(id::receive_maximum, b, e) {}

    std::uint16_t val() const {
        return make_uint16_t(buf_.begin(), buf_.end());
    }
};


class topic_alias_maximum : public detail::n_bytes_property<2> {
public:
    using recv = topic_alias_maximum;
    topic_alias_maximum(std::uint16_t val)
        : detail::n_bytes_property<2>(id::topic_alias_maximum, { num_to_2bytes(static_cast<std::uint16_t>(val)) } ) {}

    template <typename It>
    topic_alias_maximum(It b, It e)
        : detail::n_bytes_property<2>(id::topic_alias_maximum, b, e) {}

    std::uint16_t val() const {
        return make_uint16_t(buf_.begin(), buf_.end());
    }
};


class topic_alias : public detail::n_bytes_property<2> {
public:
    using recv = topic_alias;
    topic_alias(std::uint16_t val)
        : detail::n_bytes_property<2>(id::topic_alias, { num_to_2bytes(static_cast<std::uint16_t>(val)) } ) {}

    template <typename It>
    topic_alias(It b, It e)
        : detail::n_bytes_property<2>(id::topic_alias, b, e) {}

    std::uint16_t val() const {
        return make_uint16_t(buf_.begin(), buf_.end());
    }
};

class maximum_qos : public detail::n_bytes_property<1> {
public:
    using recv = maximum_qos;
    maximum_qos(std::uint8_t qos)
        : detail::n_bytes_property<1>(id::maximum_qos, { static_cast<char>(qos) } ) {
        if (qos != qos::at_most_once &&
            qos != qos::at_least_once &&
            qos != qos::exactly_once) throw property_parse_error();
    }

    template <typename It>
    maximum_qos(It b, It e)
        : detail::n_bytes_property<1>(id::maximum_qos, b, e) {}

    std::uint8_t val() const {
        return static_cast<std::uint8_t>(buf_[0]);
    }

    static constexpr const detail::ostream_format of_ = detail::ostream_format::int_cast;
};

class retain_available : public detail::n_bytes_property<1> {
public:
    using recv = retain_available;
    retain_available(bool value)
        : detail::n_bytes_property<1>(id::retain_available, { value ? char(1) : char(0) } ) {}

    template <typename It>
    retain_available(It b, It e)
        : detail::n_bytes_property<1>(id::retain_available, b, e) {}

    bool val() const {
        return buf_[0] == 1;
    }
};


class user_property;

namespace detail {

struct len_str;

struct len_str_ref {
    explicit len_str_ref(string_view v)
        : len{num_to_2bytes(static_cast<std::uint16_t>(v.size()))}
        , str(as::buffer(v.data(), v.size()))
    {}
    len_str_ref(len_str const& v);

    std::size_t size() const {
        return len.size() + get_size(str);
    }
    boost::container::static_vector<char, 2> len;
    as::const_buffer str;
};

struct len_str {
    explicit len_str(string_view v)
        : len{num_to_2bytes(static_cast<std::uint16_t>(v.size()))}
        , str(v.data(), v.size())
    {}
    len_str(len_str_ref const& v);

    std::size_t size() const {
        return len.size() + str.size();
    }
    boost::container::static_vector<char, 2> len;
    std::string str;
};

inline len_str_ref::len_str_ref(len_str const& v)
    :len(v.len), str(as::buffer(v.str)) {}

inline len_str::len_str(len_str_ref const& v)
    :len(v.len), str(get_pointer(v.str), get_size(v.str)) {}

} // namespace detail

class user_property_ref {
public:
    using recv = user_property_ref;
    user_property_ref(string_view key, string_view val)
        : key_(key), val_(val) {}
    user_property_ref(user_property const& v);

    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(key_.len.data(), key_.len.size()));
        v.emplace_back(key_.str);
        v.emplace_back(as::buffer(val_.len.data(), val_.len.size()));
        v.emplace_back(val_.str);
    }

    template <typename It>
    void fill(It b, It e) const {
        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());

        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        {
            std::copy(key_.len.begin(), key_.len.end(), b);
            b += static_cast<typename It::difference_type>(key_.len.size());
            auto ptr = get_pointer(key_.str);
            auto size = get_size(key_.str);
            std::copy(ptr, ptr + size, b);
            b += static_cast<typename It::difference_type>(size);
        }
        {
            std::copy(val_.len.begin(), val_.len.end(), b);
            b += static_cast<typename It::difference_type>(val_.len.size());
            auto ptr = get_pointer(val_.str);
            auto size = get_size(val_.str);
            std::copy(ptr, ptr + size, b);
            b += static_cast<typename It::difference_type>(size);
        }
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return
            1 + // id_
            key_.size() +
            val_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return
            1 + // header
            2 + // key (len, str)
            2;  // val (len, str)
    }

    string_view key() const {
        return string_view(get_pointer(key_.str), get_size(key_.str));
    }

    string_view val() const {
        return string_view(get_pointer(val_.str), get_size(val_.str));
    }

    static constexpr detail::ostream_format const of_ = detail::ostream_format::key_val;
private:
    friend class user_property;
    property::id id_ = id::user_property;
    detail::len_str_ref key_;
    detail::len_str_ref val_;
};

class user_property {
public:
    using recv = user_property_ref;
    user_property(string_view key, string_view val)
        : key_(key), val_(val) {}
    user_property(user_property_ref const& v);

    /**
     * @brief Add const buffer sequence into the given buffer.
     * @param v buffer to add
     */
    void add_const_buffer_sequence(std::vector<as::const_buffer>& v) const {
        v.emplace_back(as::buffer(&id_, 1));
        v.emplace_back(as::buffer(key_.len.data(), key_.len.size()));
        v.emplace_back(as::buffer(key_.str.data(), key_.str.size()));
        v.emplace_back(as::buffer(val_.len.data(), val_.len.size()));
        v.emplace_back(as::buffer(val_.str.data(), val_.str.size()));
    }

    template <typename It>
    void fill(It b, It e) const {
        BOOST_ASSERT(static_cast<std::size_t>(std::distance(b, e)) >= size());

        *b++ = static_cast<typename std::iterator_traits<It>::value_type>(id_);
        {
            std::copy(key_.len.begin(), key_.len.end(), b);
            b += static_cast<typename It::difference_type>(key_.len.size());
            auto ptr = key_.str.data();
            auto size = key_.str.size();
            std::copy(ptr, ptr + size, b);
            b += static_cast<typename It::difference_type>(size);
        }
        {
            std::copy(val_.len.begin(), val_.len.end(), b);
            b += static_cast<typename It::difference_type>(val_.len.size());
            auto ptr = val_.str.data();
            auto size = val_.str.size();
            std::copy(ptr, ptr + size, b);
            b += static_cast<typename It::difference_type>(size);
        }
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return
            1 + // id_
            key_.size() +
            val_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return
            1 + // header
            2 + // key (len, str)
            2;  // val (len, str)
    }

    string_view key() const {
        return key_.str;
    }

    string_view val() const {
        return val_.str;
    }

    static constexpr detail::ostream_format const of_ = detail::ostream_format::key_val;
private:
    friend class user_property_ref;
    property::id id_ = id::user_property;
    detail::len_str key_;
    detail::len_str val_;
};

inline user_property_ref::user_property_ref(user_property const& v)
    :id_(v.id_), key_(v.key_), val_(v.val_) {}

inline user_property::user_property(user_property_ref const& v)
    :id_(v.id_), key_(v.key_), val_(v.val_) {}

class maximum_packet_size : public detail::n_bytes_property<4> {
public:
    using recv = maximum_packet_size;
    maximum_packet_size(std::uint32_t val)
        : detail::n_bytes_property<4>(id::maximum_packet_size, { num_to_4bytes(static_cast<std::uint32_t>(val)) } ) {}

    template <typename It>
    maximum_packet_size(It b, It e)
        : detail::n_bytes_property<4>(id::maximum_packet_size, b, e) {}

    std::uint32_t val() const {
        return make_uint32_t(buf_.begin(), buf_.end());
    }
};


class wildcard_subscription_available : public detail::n_bytes_property<1> {
public:
    using recv = wildcard_subscription_available;
    wildcard_subscription_available(bool value)
        : detail::n_bytes_property<1>(id::wildcard_subscription_available, { value ? char(1) : char(0) } ) {}

    template <typename It>
    wildcard_subscription_available(It b, It e)
        : detail::n_bytes_property<1>(id::wildcard_subscription_available, b, e) {}

    bool val() const {
        return buf_[0] == 1;
    }
};


class subscription_identifier_available : public detail::n_bytes_property<1> {
public:
    using recv = subscription_identifier_available;
    subscription_identifier_available(bool value)
        : detail::n_bytes_property<1>(id::subscription_identifier_available, { value ? char(1) : char(0) } ) {}

    template <typename It>
    subscription_identifier_available(It b, It e)
        : detail::n_bytes_property<1>(id::subscription_identifier_available, b, e) {}

    bool val() const {
        return buf_[0] == 1;
    }
};


class shared_subscription_available : public detail::n_bytes_property<1> {
public:
    using recv = shared_subscription_available;
    shared_subscription_available(bool value)
        : detail::n_bytes_property<1>(id::shared_subscription_available, { value ? char(1) : char(0) } ) {}

    template <typename It>
    shared_subscription_available(It b, It e)
        : detail::n_bytes_property<1>(id::shared_subscription_available, b, e) {}

    bool val() const {
        return buf_[0] == 1;
    }
};

template <typename Property>
typename std::enable_if<
    Property::of_ == detail::ostream_format::direct,
    std::ostream&
>::type
operator<<(std::ostream& o, Property const& p) {
    o << p.val();
    return o;
}

template <typename Property>
typename std::enable_if<
    Property::of_ == detail::ostream_format::int_cast,
    std::ostream&
>::type
operator<<(std::ostream& o, Property const& p) {
    o << static_cast<int>(p.val());
    return o;
}

template <typename Property>
typename std::enable_if<
    Property::of_ == detail::ostream_format::key_val,
    std::ostream&
>::type
operator<<(std::ostream& o, Property const& p) {
    o << p.key() << ':' << p.val();
    return o;
}

template <typename Property>
typename std::enable_if<
    Property::of_ == detail::ostream_format::binary_string,
    std::ostream&
>::type
operator<<(std::ostream& o, Property const& p) {
    o << (p.val() == payload_format_indicator::binary ? "binary" : "string");
    return o;
}


} // namespace property
} // namespace v5
} // namespace mqtt

#endif // MQTT_PROPERTY_HPP
