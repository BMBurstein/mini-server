#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "asio.hpp"

#include "connection_base.hpp"

namespace bb {
	class Router;

	class Connection : public http_connection_base<Connection>
	{
	public:
		template<typename ... T>
		static ptr new_connection(T&& ... all) {
			return std::shared_ptr<Connection>(new Connection(std::forward<T>(all)...));
		}

		void start() { get_req(); }

		template<typename T>
		auto send_response(T resp) -> decltype(asio::buffer(resp), void()) {
			auto ptr = std::make_unique<T>(std::move(resp));
			auto buf = asio::buffer(*ptr);
			asio::async_write(socket, buf, [this, self{ shared_from_this() }, ptr{ std::move(ptr) }](auto ec, auto) {
				if (handle_error(ec)) {
					get_req(); // reuse connection
				}
			});
		}

		void send_response(asio::streambuf resp) {
			auto ptr = std::make_unique<asio::streambuf>(std::move(resp));
			auto& buf = *ptr;
			asio::async_write(socket, buf, [this, self{ shared_from_this() }, ptr{ std::move(ptr) }](auto ec, auto) {
				if (handle_error(ec)) {
					get_req(); // reuse connection
				}
			});
		}

		void send_response(int status, std::string const& headers, std::string const& body) {
			asio::streambuf resp_buf;
			std::ostream os(&resp_buf);
			os << "HTTP/1.1 " << status << " STAT\r\nContent-Length: " << body.length() << "\r\n" << headers << "\r\n" << body;
			send_response(std::move(resp_buf));
		}

	private:
		friend http_connection_base<Connection>;

		Connection(asio::ip::tcp::socket socket, Router const& router) : http_connection_base(std::move(socket)), router(router) { }

		void get_req() {
			asio::async_read_until(socket, buf_in, "\r\n", [this, self{ shared_from_this() }](auto ec, auto) {
				if (handle_error(ec)) {
					std::istream is(&buf_in);
					std::string line;
					std::getline(is, line);

					std::smatch parts;
					if (std::regex_match(line, parts, re_req)) {
						method = parts[1];
						uri = url_decode(parts[2]);
						get_headers();
					}
					else {
						// not HTTP, just close connection
					}
				}
			});
		}

		static char from_hex(char c) {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'A' && c <= 'F') return c - 'A' + 0xA;
			if (c >= 'a' && c <= 'f') return c - 'a' + 0xA;
			return 0;
		}

		static std::string url_decode(std::string const& url) {
			std::string dec;
			dec.reserve(url.length());
			auto pe = url.end();
			for (auto p = url.begin(); p != pe; ++p) {
				if (*p == '%') {
					char c = from_hex(*++p) << 4;
					c |= from_hex(*++p);
					dec += c;
				}
				else {
					dec += *p;
				}
			}
			return dec;
		}

		void handle_body();

		void respond(int stat) {
			send_response(stat, "", "");
		}

		bool handle_error(asio::error_code err) {
			if (!err)
				return true;
			if (err == asio::error::eof)
				return true;
			if (err != asio::error::operation_aborted) {
				respond(500);
				std::cerr << err.message() << '\n';
			}
			return false;
		}

		void handle_parse_error() {
			respond(400);
		}

		void make_test_response() {
			std::ostringstream os;
			os << method << ' ' << uri << '\n';
			for (const auto& h : rcv_headers) {
				os << h.first << " = " << h.second << '\n';
			}
			os << "Body length: " << rcv_body.size() << '\n';
			os << std::hex;
			for (const auto c : rcv_body) {
				os << c;
			}
			send_response(404, "", os.str());
		}

		std::string method, uri;
		Router const& router;

		const static std::regex re_req;
	}; // class Connection
} // namespace bb

#include "router.hpp"

namespace bb {
	inline void Connection::handle_body() {
		if (!router.handle_route(uri, method, shared_from_this())) {
			make_test_response();
		}
	}

	// The '\r' at the end is needed since getline only strips the '\n'
	const std::regex Connection::re_req(R"(([A-Z]+) ([-/%.+?=&\w]+) HTTP/1.[10]\r)", std::regex::optimize);
} // namespace bb
