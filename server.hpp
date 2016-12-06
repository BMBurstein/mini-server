#pragma once

#include <iostream>
#include <vector>
#include <thread>

#include "asio.hpp"

#include "connection.hpp"
#include "router.hpp"

class Server
{
public:
	Server(unsigned short port = 80) : strand(io), signals(io), acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
		signals.add(SIGINT);
		signals.add(SIGTERM);
#if defined(SIGQUIT)
		signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
		signals.async_wait(asio::bind_executor(strand, [this](auto, auto) {
			std::cerr << "stopping... ";
			acceptor.close();
		}));

		do_accept();
	}

	void run(unsigned int num_threads = (std::thread::hardware_concurrency()-1)) {
		if (num_threads == 0)
			num_threads = 1;
		std::cout << "Starting server using " << num_threads << " threads\n";
		for (unsigned int i = 0; i < num_threads; ++i) {
			run_pool.emplace_back([this]() { io.run(); });
		}
		for (unsigned int i = 0; i < num_threads; ++i) {
			run_pool[i].join();
		}
	}

	Router& get_router() { return router; }

private:
	void do_accept() {
		acceptor.async_accept(asio::bind_executor(strand, [this](auto err, asio::ip::tcp::socket&& socket) {
			if (!acceptor.is_open()) {
				std::cerr << "Stopped\n";
				return;
			}
			if (!err) {
				Connection::new_connection(std::move(socket), router)->start();
				// `this->` is not actually needed, but works around a bug in gcc
				this->do_accept();
			}
			else {
				std::cerr << err.message() << '\n';
			}
		}));
	}

	asio::io_service io;
	asio::io_service::strand strand;
	asio::signal_set signals;
	asio::ip::tcp::acceptor acceptor;
	std::vector<std::thread> run_pool;
	Router router;
};
