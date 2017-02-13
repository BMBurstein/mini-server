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
			handler = func;
			auto ptr = std::make_unique<asio::streambuf>(std::move(resp));
			auto& buf = *ptr;
			asio::async_write(socket, buf, [this, self{ shared_from_this() }, ptr{ std::move(ptr) }](auto ec, auto) {
				if (handle_error(ec)) {
					get_resp();
				}
			});
		}

		void send_request(std::string const& uri, std::string const& headers, HandlerFunc func, Methods method = Methods::GET, std::string const& body = "") {
			asio::streambuf resp_buf;
			std::ostream os(&resp_buf);
			os << name_from_method(method) << ' ' << uri << " HTTP/1.1\r\nHost: " << host << "\r\nContent-Length: " << body.length() << "\r\n" << headers << "\r\n" << body;
			send_request(std::move(resp_buf), func);
		}

		unsigned int status() { return stat; }

	private:
		friend http_connection_base<ClientConnection>;

		ClientConnection(asio::io_context& context, std::string host_url, std::string const& port) : http_connection_base(asio::ip::tcp::socket(context)), endpoints(asio::ip::tcp::resolver(context).resolve(host_url, port)), host(std::move(host_url)) {
			asio::error_code ec;
			asio::connect(socket, endpoints, ec);
			if (ec) {
				std::cerr << ec.message();
			}
		}

		void get_resp() {
			asio::async_read_until(socket, buf_in, "\r\n", [this, self{ shared_from_this() }](auto ec, auto) {
				if (handle_error(ec)) {
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

		unsigned int stat;
		HandlerFunc handler;

		const static std::regex re_resp;
	}; //class ClientConnection

	// The '\r' at the end is needed since getline only strips the '\n'
	const std::regex ClientConnection::re_resp(R"(HTTP/1.[10] (\d{3}) .+\r)", std::regex::optimize);
} // namespace bb
