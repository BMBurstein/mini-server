#pragma once

#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "asio.hpp"

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
			return std::lexicographical_compare
			(s1.begin(), s1.end(),   // source range
				s2.begin(), s2.end(),   // dest range
				nocase_compare());  // comparison
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
		asio::error_code ec;
		socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		socket.close(ec);
	}

	void start() { get_req(); }

	void send_response(int status, std::string const& headers, std::string const& body) {
		std::ostream os(&resp_buf);

		os << "HTTP/1.1 " << status << " \r\n" << headers << "\r\n" << body;

		auto self(shared_from_this());
		asio::async_write(socket, resp_buf, [this, self](auto ec, auto) {
			if (this->handle_error(ec)) {

			}
		});
	}

	HeaderMap const& getHeaders() { return req_headers; }
	DATA const& getBody() { return req_body; }

private:
	Connection(asio::ip::tcp::socket socket, Router const& router) : socket(std::move(socket)), router(router) { }

	void get_req() {
		auto self(shared_from_this());
		// passing `self` by val to the lambda ensures that the shared pointer doesn't destroy it before the lambda gets called
		asio::async_read_until(socket, buf_in, "\r\n", [this, self](auto ec, auto) {
			if (this->handle_error(ec)) {
				std::istream is(&buf_in);
				std::string line;
				std::getline(is, line);

				std::smatch parts;
				if (std::regex_match(line, parts, re_req)) {
					method = parts[1];
					uri = parts[2];
					this->get_headers();
				}
				else {
					this->respond(400);
				}
			}
		});
	}

	void get_headers() {
		auto self(shared_from_this());
		// passing `self` by val to the lambda ensures that the shared pointer doesn't destroy it before the lambda gets called
		asio::async_read_until(socket, buf_in, "\r\n", [this, self](auto ec, auto) {
			if (this->handle_error(ec)) {
				std::istream is(&buf_in);
				std::string line;
				std::getline(is, line);

				std::smatch parts;
				if (std::regex_match(line, parts, re_head)) {
					req_headers[parts[1]] = parts[2];
					this->get_headers();
				}
				else if (line == "\r") {
					this->get_body();
				}
				else {
					this->respond(400);
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
		//os << "HTTP/1.1 200 OK\r\n\r\n";
		os << method << ' ' << uri << '\n';
		for (const auto& h : req_headers) {
			os << h.first << " = " << h.second << '\n';
		}
		os << "Body length: " << req_body.size() << '\n';
		os << std::hex;
		for (const auto c : req_body) {
			os << c;
		}
		send_response(500, "", os.str());
	}

	asio::streambuf resp_buf;

	std::string method, uri;
	HeaderMap req_headers;
	DATA req_body;

	asio::streambuf buf_in;
	asio::ip::tcp::socket socket;
	Router const& router;

	const static std::regex re_req;
	const static std::regex re_head;
};

#include "router.hpp"

inline void Connection::get_body() {
	std::size_t len = 0;
	auto it = req_headers.find("content-length");
	if (it != req_headers.end()) {
		try {
			len = std::stoull(it->second.c_str());
		}
		catch (const std::exception&) {}
	}
	req_body.resize(len);
	auto have_len = buf_in.size();
	asio::buffer_copy(asio::buffer(req_body), buf_in.data());
	buf_in.consume(have_len);
	auto body_buf = asio::buffer(req_body.data() + have_len, len - have_len);

	auto self(shared_from_this());
	// passing `self` by val to the lambda ensures that the shared pointer doesn't destroy it before the lambda gets called
	asio::async_read(socket, body_buf, [this, self](auto ec, auto) {
		if (this->handle_error(ec)) {
			if (!router.handle_route(uri, method, self)) {
				this->make_test_response();
			}
		}
	});
}

// The '\r' at the end is needed since getline only strips the '\n'
const std::regex Connection::re_req(R"(([A-Z]+) ([-/%.+?=&\w]+) HTTP/1.[10]\r)", std::regex::optimize);
const std::regex Connection::re_head(R"(([-\w]+): (.*)\r)", std::regex::optimize);
