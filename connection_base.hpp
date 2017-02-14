#pragma once

#include <map>
#include <memory>
#include <regex>

#include "asio.hpp"

namespace bb {

	template<typename T>
	class http_connection_base : public std::enable_shared_from_this<T> {
	protected:
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
		typedef std::map<std::string, std::string, IgnoreCaseLT> HeaderMap;
		typedef std::vector<char> DATA;
		typedef std::shared_ptr<T> ptr;

		HeaderMap const& headers() { return rcv_headers; }
		DATA const& body() { return rcv_body; }

	protected:
		HeaderMap rcv_headers;
		DATA rcv_body;

		asio::streambuf buf_in;
		asio::ip::tcp::socket socket;

		http_connection_base(asio::ip::tcp::socket socket) : socket(std::move(socket)) { }

		void get_headers() {
			asio::async_read_until(socket, buf_in, "\r\n", [this, self{ this->shared_from_this() }](auto ec, auto) {
				if (self->handle_error(ec)) {
					std::istream is(&buf_in);
					std::string line;
					std::getline(is, line);

					std::smatch parts;
					if (std::regex_match(line, parts, re_head)) {
						rcv_headers[parts[1]] = parts[2];
						get_headers();
					}
					else if (line == "\r") {
						get_body();
					}
					else {
						self->handle_parse_error();
					}
				}
			});
		}

		void get_body() {
			std::size_t len = 0;
			auto it = rcv_headers.find("content-length");
			if (it != rcv_headers.end()) {
				try {
					len = std::stoul(it->second.c_str());
				}
				catch (const std::exception&) {}
			}
			rcv_body.resize(len);
			auto have_len = buf_in.size();
			asio::buffer_copy(asio::buffer(rcv_body), buf_in.data());
			buf_in.consume(have_len);
			auto body_buf = asio::buffer(rcv_body.data() + have_len, len - have_len);

			asio::async_read(socket, body_buf, [this, self{ this->shared_from_this() }](auto ec, auto) {
				if (self->handle_error(ec)) {
					self->handle_body();
				}
			});
		}

		const static std::regex re_head;
	};

	// The '\r' at the end is needed since getline only strips the '\n'
	template<typename T>
	const std::regex http_connection_base<T>::re_head(R"(([-\w]+): (.*)\r)", std::regex::optimize);
}
