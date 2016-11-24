#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <regex>
#include <map>
#include <utility>
#include <thread>

#include "asio.hpp"

using asio::ip::tcp;


class Connection : public std::enable_shared_from_this<Connection>
{
public:
	typedef std::shared_ptr<Connection> ptr;

	template<typename ... T>
	static ptr new_connection(T&& ... all) {
		return std::shared_ptr<Connection>(new Connection(std::forward<T>(all)...));
	}

	~Connection() {
		asio::error_code ec;
		socket.shutdown(tcp::socket::shutdown_both, ec);
		socket.close(ec);
	}

	void start() { get_req(); }

private:
	Connection(tcp::socket socket) : socket(std::move(socket)) { }

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
					headers[parts[1]] = parts[2];
					this->get_headers();
				}
				else if (line == "\r") {
					this->print();
					this->respond(200);
				}
				else {
					this->respond(400);
				}
			}
			else {
				this->respond(500);
			}
		});
	}

	void respond(int stat) {
		auto self(shared_from_this());
		// passing `self` by val to the lambda ensures that the shared pointer doesn't destroy it before the lambda gets called
		asio::async_write(socket, buf_out, [this, self](auto ec, auto) {
			if (this->handle_error(ec)) {

			}
		});
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

	void print() {
		std::ostream os(&buf_out);
		os << "HTTP/1.1 200 OK\r\n\r\n";
		os << method << ' ' << uri << '\n';
		for (const auto& h : headers) {
			os << h.first << " = " << h.second << '\n';
		}
	}

	std::string method, uri;
	std::map<std::string, std::string> headers;

	tcp::socket socket;
	asio::streambuf buf_in;
	asio::streambuf buf_out;

	const static std::regex re_req;
	const static std::regex re_head;
};

// The '\r' at the end is needed since getline only strips the '\n'
const std::regex Connection::re_req(R"(([A-Z]+) ([-/%.+?=&\w]+) HTTP/1.[10]\r)", std::regex::optimize);
const std::regex Connection::re_head(R"(([-\w]+): (.*)\r)", std::regex::optimize);

class Server
{
public:
	Server() : acceptor(io, tcp::endpoint(tcp::v4(), 80)) {
		do_accept();
	}

	void run() {
		auto num_threads = std::thread::hardware_concurrency();
		for (unsigned int i = 0; i < num_threads; ++i) {
			run_pool.emplace_back([this]() { io.run(); });
		}
		for (unsigned int i = 0; i < num_threads; ++i) {
			run_pool[i].join();
		}
	}

private:
	void do_accept() {
		acceptor.async_accept([this](auto err, auto socket) {
			if (!err) {
				Connection::new_connection(std::move(socket))->start();
				// `this->` is not actually needed, but works around a bug in gcc
				this->do_accept();
			}
			else {
				std::cerr << err.message() << '\n';
			}
		});
	}

	asio::io_service io;
	tcp::acceptor acceptor;
	std::vector<std::thread> run_pool;
};

int main()
{
	Server s;
	s.run();

	return 0;
}
