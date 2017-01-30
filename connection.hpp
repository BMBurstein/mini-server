#pragma once

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "asio.hpp"

namespace bb {
	class Router;

	class Connection : public std::enable_shared_from_this<Connection>
	{
	private:
		// source: http://stackoverflow.com/a/1801913/331785
		struct IgnoreCaseLT {
			// case-independent (ci) compare_less binary function
			struct nocase_compare
			{
				bool operator() (const unsigned char& c1, const unsigned char& c2) const {
					return tolower(c1) < tolower(c2);
				}
			};
			bool operator() (const std::string & s1, const std::string & s2) const {
				return std::lexicographical_compare(s1.begin(), s1.end(),   // source range
				                                    s2.begin(), s2.end(),   // dest range
				                                    nocase_compare());      // comparison
			}
		};

	public:
		typedef std::shared_ptr<Connection> ptr;
		typedef std::map<std::string, std::string, IgnoreCaseLT> HeaderMap;
		typedef std::vector<char> DATA;

		template<typename ... T>
		static ptr new_connection(T&& ... all) {
			return std::shared_ptr<Connection>(new Connection(std::forward<T>(all)...));
		}

		~Connection() {
			shutdown();

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

		HeaderMap const& getHeaders() { return req_headers; }
		DATA const& getBody() { return req_body; }

	private:
		Connection(asio::ip::tcp::socket socket, Router const& router) : socket(std::move(socket)), router(router), reset_timer(this->socket.get_executor().context()) { }

		void start_timer() {
			reset_timer.expires_after(std::chrono::seconds(5));
			reset_timer.async_wait([this, self{ shared_from_this() }](auto const& ec){
				if (!ec) {
					shutdown();
				}
			});
		}

		void shutdown()
		{
			asio::error_code ec;
			socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
			socket.close(ec);
		}

		void get_req() {
			start_timer();
			asio::async_read_until(socket, buf_in, "\r\n", [this, self{ shared_from_this() }](auto ec, auto) {
				if (handle_error(ec)) {
					std::istream is(&buf_in);
					std::string line;
					std::getline(is, line);

					std::smatch parts;
					if (std::regex_match(line, parts, re_req)) {
						method = parts[1];
						uri = parts[2];
						get_headers();
					}
					else {
						// not HTTP, just close connection
					}
				}
			});
		}

		void get_headers() {
			asio::async_read_until(socket, buf_in, "\r\n", [this, self{ shared_from_this() }](auto ec, auto) {
				if (handle_error(ec)) {
					std::istream is(&buf_in);
					std::string line;
					std::getline(is, line);

					std::smatch parts;
					if (std::regex_match(line, parts, re_head)) {
						req_headers[parts[1]] = parts[2];
						get_headers();
					}
					else if (line == "\r") {
						get_body();
					}
					else {
						respond(400);
					}
				}
			});
		}

		void get_body();

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

		void make_test_response() {
			std::ostringstream os;
			os << method << ' ' << uri << '\n';
			for (const auto& h : req_headers) {
				os << h.first << " = " << h.second << '\n';
			}
			os << "Body length: " << req_body.size() << '\n';
			os << std::hex;
			for (const auto c : req_body) {
				os << c;
			}
			send_response(404, "", os.str());
		}

		std::string method, uri;
		HeaderMap req_headers;
		DATA req_body;

		asio::streambuf buf_in;
		asio::ip::tcp::socket socket;
		Router const& router;
		asio::steady_timer reset_timer;

		const static std::regex re_req;
		const static std::regex re_head;
	}; // class Connection
} // namespace bb

#include "router.hpp"

namespace bb {
	inline void Connection::get_body() {
		std::size_t len = 0;
		auto it = req_headers.find("content-length");
		if (it != req_headers.end()) {
			try {
				len = std::stoul(it->second.c_str());
			}
			catch (const std::exception&) {}
		}
		req_body.resize(len);
		auto have_len = buf_in.size();
		asio::buffer_copy(asio::buffer(req_body), buf_in.data());
		buf_in.consume(have_len);
		auto body_buf = asio::buffer(req_body.data() + have_len, len - have_len);

		asio::async_read(socket, body_buf, [this, self{ shared_from_this() }](auto ec, auto) {
			if (handle_error(ec)) {
				if (!router.handle_route(uri, method, std::move(self))) {
					make_test_response();
				}
			}
		});
	}

	// The '\r' at the end is needed since getline only strips the '\n'
	const std::regex Connection::re_req(R"(([A-Z]+) ([-/%.+?=&\w]+) HTTP/1.[10]\r)", std::regex::optimize);
	const std::regex Connection::re_head(R"(([-\w]+): (.*)\r)", std::regex::optimize);
} // namespace bb
