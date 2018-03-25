// Copyright Takatoshi Kondo 2018
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SHARED_CONST_BUFFER_SEQUENCE_HPP)
#define MQTT_SHARED_CONST_BUFFER_SEQUENCE_HPP

#include <string>
#include <vector>
#include <memory>
#include <boost/asio.hpp>

namespace mqtt {

namespace as = boost::asio;

    /**
     * @brief Restore serialized publish and pubrel messages.
     *        This function shouold be called before connect.
     * @param packet_id packet id of the message
     * @param scbs      shared_const_buffer_sequence to restore
     */
class shared_const_buffer_sequence {
public:
    /**
     * @brief Create empty shared_const_buffer_sequence.
     */
    shared_const_buffer_sequence() = default;

    /**
     * @brief Create shared_const_buffer_sequence from shared_ptr of string.
     * @param buf initial buffer.
     */
    shared_const_buffer_sequence(std::shared_ptr<std::string> const& buf)
        :bufs_{ buf }
    {}

    /**
     * @brief Create shared_const_buffer_sequence from shared_ptr of string (move).
     * @param buf initial buffer.
     */
    shared_const_buffer_sequence(std::shared_ptr<std::string>&& buf)
        :bufs_{ std::move(buf) }
    {}

    /**
     * @brief Create shared_const_buffer_sequence from string.
     * @param buf initial buffer.
     */
    shared_const_buffer_sequence(std::string const& buf)
        :bufs_{ std::make_shared<std::string>(buf) }
    {}

    /**
     * @brief Create shared_const_buffer_sequence from string (move).
     * @param buf initial buffer.
     */
    shared_const_buffer_sequence(std::string&& buf)
        :bufs_{ std::make_shared<std::string>(std::move(buf)) }
    {}
    shared_const_buffer_sequence(shared_const_buffer_sequence const&) = default;
    shared_const_buffer_sequence(shared_const_buffer_sequence&&) = default;
    shared_const_buffer_sequence& operator=(shared_const_buffer_sequence const&) = default;
    shared_const_buffer_sequence& operator=(shared_const_buffer_sequence&&) = default;

    /**
     * @brief Reserve internal buffer.
     * @param size    reserve size
     */
    void reserve(std::size_t size) {
        bufs_.reserve(size);
    }

    /**
     * @brief Add buffer to sequence
     * @param buf the buffer to add.
     */
    void add_buffer(std::shared_ptr<std::string> const& buf) {
        bufs_.push_back(buf);
    }

    /**
     * @brief Add buffer to sequence
     * @param buf the buffer to add.
     */
    void add_buffer(std::shared_ptr<std::string>&& buf) {
        bufs_.push_back(std::move(buf));
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> create_const_buffer_sequence() const {
        std::vector<as::const_buffer> ret;
        ret.reserve(bufs_.size());
        for (auto const& buf : bufs_) {
            ret.emplace_back(buf->data(), buf->size());
        }
        return ret;
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        std::size_t size = 0;
        for (auto const& buf : bufs_) {
            size += buf->size();
        }
        return size;
    }

    /**
     * @brief Create one continuours buffer.
     *        All sequence of buffers are concatinated.
     *        It is useful to store to file/database.
     * @return continuous buffer
     */
    std::string create_continuous_buffer() const {
        std::size_t s = size();
        std::string ret;
        ret.reserve(s);
        for (auto const& buf : bufs_) {
            ret.insert(ret.size(), *buf);
        }
        return ret;
    }
private:
    std::vector<std::shared_ptr<std::string>> bufs_;
};

} // namespace mqtt

#endif // MQTT_SHARED_CONST_BUFFER_SEQUENCE_HPP
