//
// Created by maxng on 12/23/2022.
//

#ifndef SR_SERVER_TEST_NET_HPP
#define SR_SERVER_TEST_NET_HPP

#include <vector>
#include <thread>
#include <map>
#include <unordered_map>
#include <iostream>
#include <variant>

#include <boost/asio.hpp>

/// Generate listener/dispatcher for events.
#define SR_DISPATCHER(listener, ...)   struct listener##_dispatcher {                                                 \
                                           using listener_##listener##_signature = std::function<void (__VA_ARGS__)>; \
                                                                                                                      \
                                       protected:                                                                     \
                                           std::vector<listener_##listener##_signature> m_listeners;                  \
                                                                                                                      \
                                           template <typename... t_args>                                              \
                                           void dispatch_##listener(t_args&&... a_args) {                             \
                                               for (auto& listener : m_listeners) {                                   \
                                                   listener(std::forward<t_args>(a_args)...);                         \
                                               }                                                                      \
                                           }                                                                          \
                                                                                                                      \
                                       public:                                                                        \
                                           void on_##listener(listener_##listener##_signature a_function) {           \
                                               m_listeners.push_back(std::move(a_function));                          \
                                           }                                                                          \
                                       };

namespace sr {
    using io_context = boost::asio::io_context;
    using acceptor = boost::asio::ip::tcp::acceptor;
    using socket = boost::asio::ip::tcp::socket;
    using error_code = boost::system::error_code;
    using port_type = boost::asio::ip::port_type;
    using resolver = boost::asio::ip::tcp::resolver;

    /// Get the current system time in nanoseconds.
    [[nodiscard]] std::uint64_t system_nano_time() {
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }

    /// Get the current system time in milliseconds.
    [[nodiscard]] std::uint64_t system_millis_time() {
        return system_nano_time() / 1000000;
    }

    enum class type : uint8_t {
        none = 0,
        integer = 1,
        string = 2
    };

    namespace client { class net; }
    namespace server { class net; }

    template <typename... t_dispatch_args>
    class net_interface {
        friend client::net;
        friend server::net;

        std::vector<uint8_t> m_buffer;
        size_t m_buffer_index = 0;

        std::vector<std::unique_ptr<std::string>> m_message_ids;
        std::unordered_map<std::string_view, size_t> m_message_id_map;
        std::map<std::string_view, std::function<void (t_dispatch_args&&...)>> m_message_handlers;
        std::unordered_map<size_t, std::function<void (t_dispatch_args&&...)>*> m_message_handlers_id_map;
        std::vector<std::string_view> m_no_send_ids;

    public:
        void clear_message_ids() {
            m_message_ids.clear();
            m_message_id_map.clear();
            m_message_handlers_id_map.clear();
            m_no_send_ids.clear();
        }

        auto& get_buffer() {
            return m_buffer;
        }

        void set_buffer_size(size_t a_size) {
            m_buffer.resize(a_size);
        }

        void add_network_string(std::string a_string, bool a_no_send = false) {
            m_message_ids.emplace_back(std::make_unique<std::string>(std::move(a_string)));
            std::string_view str = *m_message_ids.back();
            m_message_id_map[str] = m_message_ids.size();

            if (a_no_send) {
                m_no_send_ids.push_back(str);
            }
        }

        void receive(std::string_view a_id, std::function<void (t_dispatch_args&&...)> a_callback) {
            auto handler_it = m_message_handlers.emplace(a_id, std::move(a_callback));

            auto it = m_message_id_map.find(a_id);

            if (it == m_message_id_map.cend()) {
                return;
            }

            m_message_handlers_id_map.emplace(it->second, &handler_it.first->second);
        }

        void start(std::string_view a_id) {
            auto id_it = m_message_id_map.find(a_id);

            if (id_it == m_message_id_map.cend()) {
                return;
            }

            m_buffer_index = 0;

            write_to_buffer(id_it->second);
        }

        void write_int(size_t a_int) {
            write_to_buffer(type::integer);
            write_to_buffer(a_int);
        }

        void write_string(std::string_view a_string) {
            write_to_buffer(type::string);
            write_to_buffer(a_string.size());
            write_to_buffer(a_string.data(), a_string.size());
        }

        size_t read_int() {
            check_next_type(type::integer);
            return read_from_buffer<size_t>();
        }

        std::string read_string() {
            check_next_type(type::string);

            auto string_size = read_from_buffer<size_t>();

            std::string string;
            string.resize(string_size);
            read_from_buffer(string.data(), string_size);

            return string;
        }

        /// Compile a schema message in the buffer containing all of the message IDs.
        void compile_schema() {
            start("NET_MESSAGE_SCHEMA");
            write_int(m_message_id_map.size() - m_no_send_ids.size());

            std::cout << (m_message_id_map.size() - m_no_send_ids.size()) << std::endl;

            for (auto& entry : m_message_id_map) {
                if (std::find(m_no_send_ids.begin(), m_no_send_ids.end(), entry.first) != m_no_send_ids.cend()) {
                    continue;
                }

                std::cout << entry.first << ' ' << entry.second << std::endl;

                write_string(entry.first);
                write_int(entry.second);
            }
        }

        void dispatch(t_dispatch_args&&... a_args) {
            m_buffer_index = 0;
            auto key = read_from_buffer<size_t>();
            (*m_message_handlers_id_map[key])(std::forward<t_dispatch_args>(a_args)...);
        }

        void reset_buffer_position() {
            m_buffer_index = 0;
        }

    private:
        void check_next_type(type a_type) {
            if (read_from_buffer<type>() != a_type) {
                throw std::invalid_argument("type of next element does not match requested type");
            }
        }

        template <typename t_type>
        void write_to_buffer(const t_type& a_value) {
            static constexpr size_t value_size = sizeof(t_type);

            if (value_size + m_buffer_index >= m_buffer.size()) {
                throw std::out_of_range("overflowed buffer storage");
            }

            std::memcpy(m_buffer.data() + m_buffer_index, &a_value, value_size);

            m_buffer_index += value_size;
        }

        template <typename t_type>
        void write_to_buffer(t_type* a_data, size_t a_size) {
            if (a_size + m_buffer_index >= m_buffer.size()) {
                throw std::out_of_range("overflowed buffer storage");
            }

            std::memcpy(m_buffer.data() + m_buffer_index, a_data, a_size);

            m_buffer_index += a_size;
        }

        template <typename t_type>
        void read_from_buffer(t_type& a_value) {
            static constexpr size_t value_size = sizeof(t_type);

            if (value_size + m_buffer_index >= m_buffer.size()) {
                throw std::out_of_range("overflowed buffer storage");
            }

            std::memcpy(reinterpret_cast<void*>(&a_value), m_buffer.data() + m_buffer_index, value_size);

            m_buffer_index += value_size;
        }

        template <typename t_type>
        void read_from_buffer(t_type* a_data, size_t a_size) {
            if (a_size + m_buffer_index >= m_buffer.size()) {
                throw std::out_of_range("overflowed buffer storage");
            }

            std::memcpy(reinterpret_cast<void*>(a_data), m_buffer.data() + m_buffer_index, a_size);

            m_buffer_index += a_size;
        }

        template <typename t_type>
        t_type read_from_buffer() {
            t_type value;
            read_from_buffer(value);
            return value;
        }
    };

    namespace client {
        SR_DISPATCHER(connect);
        SR_DISPATCHER(disconnect);
        SR_DISPATCHER(message);
        SR_DISPATCHER(ready);

        class net :
            public net_interface<>,
            public connect_dispatcher,
            public disconnect_dispatcher,
            public message_dispatcher,
            public ready_dispatcher
        {
            io_context m_context;
            resolver m_resolver;
            socket m_socket;
            bool m_connected = false;

            std::thread m_thread;
            bool m_running = false;

        public:
            explicit net() : m_context(), m_resolver(m_context), m_socket(m_context) {
                set_buffer_size(8192);

                add_network_string("NET_MESSAGE_SCHEMA", true);
                add_network_string("NET_SIGNAL_READY", true);

                receive("NET_MESSAGE_SCHEMA", [this]() {
                    clear_message_ids();

                    add_network_string("NET_MESSAGE_SCHEMA", true);
                    add_network_string("NET_SIGNAL_READY", true);

                    size_t count = read_int();

                    while (count--) {
                        m_message_ids.push_back(std::make_unique<std::string>(read_string()));
                        std::string_view str = *m_message_ids.back();

                        size_t id = read_int();

                        m_message_id_map.emplace(str, id);

                        auto handler_it = m_message_handlers.find(str);

                        if (handler_it != m_message_handlers.cend()) {
                            m_message_handlers_id_map.emplace(id, &handler_it->second);
                        }
                    }

                    dispatch_ready();

                    start("NET_SIGNAL_READY");
                    send();
                });
            }

            void connect(const std::string& a_hostname, port_type a_port) {
                error_code ec;
                boost::asio::connect(m_socket, m_resolver.resolve(a_hostname, std::to_string(a_port)), ec);
                dispatch_connect();

                begin_accept_message();

                m_connected = true;
            }

            void start_async() {
                m_running = true;
                m_thread = std::thread(&net::run, this);
            }

            void stop_async() {
                m_running = false;
                m_thread.join();
            }

            void start_sync() {
                m_running = true;
                run();
            }

            void send() {
                boost::asio::write(m_socket, boost::asio::buffer(get_buffer()));
            }

        private:
            void begin_accept_message() {
                m_socket.async_read_some(
                        boost::asio::buffer(get_buffer()),
                        [this](boost::system::error_code a_ec, std::size_t a_bytes_transferred) {
                            std::cout << a_bytes_transferred << " transferred." << std::endl;

                            if (a_ec == boost::asio::error::connection_reset) {
                                // Detected client disconnect.

                                m_connected = false;
                                dispatch_disconnect();
                                return;
                            }

                            if (!a_ec.failed()) {
                                dispatch();
                            }

                            begin_accept_message();
                        }
                );
            }

            void run() {
                while (m_running) {
                    m_context.poll_one();
                }
            }
        };
    }

    namespace server {
        class net;

        class client {
            friend class net;

            socket m_socket;
            size_t m_id;

        public:
            client(socket a_socket, size_t a_id) : m_socket(std::move(a_socket)), m_id(a_id) {}

            [[nodiscard]] bool operator == (const client& a_rhs) const noexcept {
                return m_id == a_rhs.m_id;
            }
        };

        SR_DISPATCHER(connect, client&);
        SR_DISPATCHER(disconnect, client&);
        SR_DISPATCHER(message, client&);
        SR_DISPATCHER(ready, client&);

        class net :
            public net_interface<client&>,
            public connect_dispatcher,
            public disconnect_dispatcher,
            public message_dispatcher,
            public ready_dispatcher
        {
            io_context m_context;
            std::unique_ptr<acceptor> m_acceptor;

            std::thread m_thread; ///< Thead to manage incoming connections and messages.
            bool m_running;       ///< Control boolean for thread.

            std::vector<std::unique_ptr<client>> m_clients; ///< List of all connected clients.
            std::mutex m_clients_guard;                     ///< Guard to synchronize changes to client list.

            size_t m_id_counter = 0; ///< Current counter for assigning connection IDs. Increments on each new connection.

        public:
            net() : m_context(), m_running(false), m_acceptor() {
                set_buffer_size(8192);

                add_network_string("NET_MESSAGE_SCHEMA", true);
                add_network_string("NET_SIGNAL_READY", true);

                receive("NET_SIGNAL_READY", [this](client& a_client) {
                    dispatch_ready(a_client);
                });
            }

            void open(port_type a_port) {
                m_acceptor = std::make_unique<acceptor>(
                    m_context,
                    boost::asio::ip::tcp::endpoint(
                        boost::asio::ip::tcp::v4(),
                        a_port
                    )
                );

                begin_accept();
            }

            void start_async() {
                m_running = true;
                m_thread = std::thread(&net::run, this);
            }

            void stop_async() {
                m_running = false;

                if (m_thread.joinable()) {
                    m_thread.join();
                }
            }

            void start_sync() {
                m_running = true;
                run();
            }

            void poll() {
                m_context.poll();
            }

            auto& get_clients() {
                return m_clients;
            }

            auto find_client_by_id(size_t a_id) {
                return std::find_if(m_clients.begin(), m_clients.end(), [a_id](const auto& element) -> bool {
                    return element->m_id == a_id;
                });
            }

            void disconnect(client& a_client) {
                error_code ec;
                a_client.m_socket.close(ec);

                auto it = find_client_by_id(a_client.m_id);

                if (it != m_clients.cend()) {
                    m_clients.erase(it);
                }
            }

            void send(client& a_client) {
                boost::asio::write(a_client.m_socket, boost::asio::buffer(get_buffer()));
            }

        private:
            void begin_accept() {
                m_acceptor->async_accept([this](error_code a_ec, socket a_socket) {
                    m_clients.push_back(std::make_unique<client>(std::move(a_socket), m_id_counter++));
                    begin_accept();
                    client& cl = *m_clients.back();
                    begin_accept_message(cl);
                    dispatch_connect(cl);

                    compile_schema();
                    send(cl);
                });
            }

            void begin_accept_message(client& a_client) {
                a_client.m_socket.async_read_some(
                    boost::asio::buffer(get_buffer()),
                    [this, &a_client](boost::system::error_code a_ec, std::size_t a_bytes_transferred) {
                        if (a_ec == boost::asio::error::connection_reset) {
                            // Detected client disconnect.

                            disconnect(a_client);
                            return;
                        }

                        if (!a_ec.failed()) {
                            dispatch(a_client);
                        }

                        begin_accept_message(a_client);
                    }
                );
            }

            void run() {
                while (m_running) {
                    m_context.poll_one();
                }
            }
        };
    }
}

#endif //SR_SERVER_TEST_NET_HPP
