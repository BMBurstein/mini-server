#pragma once

#include <string>
#include <iostream>
#include <memory>
#include <regex>

#include "asio.hpp"

#include "connection_base.hpp"
#include "methods.hpp"

namespace bb {
	class ClientConnection : public http_connection_base<ClientConnection>
	{
	public:
		typedef std::function<void(ptr)> HandlerFunc;

		template<typename ... T>
		static ptr new_connection(T&& ... all) {
			return std::shared_ptr<ClientConnection>(new ClientConnection(std::forward<T>(all)...));
		}

		void send_request(asio::streambuf resp, HandlerFunc func) {
			handler = std::move(func);
			send_buf = std::move(resp);
			retries = 1;
			send_req_impl();
		}

		void send_request(std::string const& uri, std::string const& headers, HandlerFunc func) {
			send_request(Methods::GET, uri, headers, "", std::move(func));
		}

		void send_request(Methods method, std::string const& uri, std::string const& headers, std::string const& body, HandlerFunc func) {
			asio::streambuf resp_buf;
			std::ostream os(&resp_buf);
			os << name_from_method(method) << ' ' << uri << " HTTP/1.1\r\nHost: " << host << "\r\nContent-Length: " << body.length() << "\r\n" << headers << "\r\n" << body;
			send_request(std::move(resp_buf), std::move(func));
		}

		unsigned int status() { return stat; }

	private:
		friend http_connection_base<ClientConnection>;

		ClientConnection(asio::io_context& context, std::string host_url, std::string const& port)
		  : http_connection_base(asio::ip::tcp::socket(context)),
			endpoints(asio::ip::tcp::resolver(context).resolve(host_url, port)),
			host(std::move(host_url))
		{ }

		void connect_send() {
			if (retries--) {
				asio::async_connect(socket, endpoints, [this, self{ shared_from_this() }](auto ec, auto) mutable {
					if (handle_error(ec)) {
						send_req_impl();
					}
				});
			}
			else {
				std::cerr << "Could not connect\n";
			}
		}

		void send_req_impl() {
			if (!socket.is_open()) {
				connect_send();
			}
			else {
				asio::async_write(socket, send_buf.data(), [this, self{ shared_from_this() }](auto ec, auto) {
					if (handle_error(ec)) {
						get_resp();
					}
				});
			}
		}

		void get_resp() {
			asio::async_read_until(socket, buf_in, "\r\n", [this, self{ shared_from_this() }](auto ec, auto) {
#if defined(ASIO_WINDOWS) || defined(__CYGWIN__)
				static const auto closed_error = asio::error::connection_aborted;
#else
				static const auto closed_error = asio::error::eof;
#endif
				if (ec == closed_error) {
					connect_send();
				}
				else if (handle_error(ec)) {
					send_buf.consume(send_buf.size());
					std::istream is(&buf_in);
					std::string line;
					std::getline(is, line);

					std::smatch parts;
					if (std::regex_match(line, parts, re_resp)) {
						stat = std::stoul(parts[1]);
						get_headers();
					}
					else {
						// not HTTP, just close connection
					}
				}
			});
		}

		bool handle_error(asio::error_code err) {
			if (!err)
				return true;
			if (err == asio::error::eof)
				return true;
			std::cerr << err.message() << '\n';
			return false;
		}

		void handle_parse_error() {
			// just close connection
		}

		void handle_body() {
			handler(shared_from_this());
		}

		asio::ip::tcp::resolver::results_type endpoints;
		std::string host;
		asio::streambuf send_buf;
		unsigned int retries;

		unsigned int stat;
		HandlerFunc handler;

		const static std::regex re_resp;
	}; //class ClientConnection

	// The '\r' at the end is needed since getline only strips the '\n'
	const std::regex ClientConnection::re_resp(R"(HTTP/1.[10] (\d{3}) .+\r)", std::regex::optimize);
} // namespace bb
