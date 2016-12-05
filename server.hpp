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
	Server(unsigned short port = 80) : signals(io), acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
		signals.add(SIGINT);
		signals.add(SIGTERM);
#if defined(SIGQUIT)
		signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
		signals.async_wait([this](auto, auto) {
			std::cerr << "stopping... ";
			stopped = true;
			acceptor.close();
		});

		do_accept();
	}

	void run() {
		auto num_threads = std::thread::hardware_concurrency();
		std::cout << "Starting server using " << num_threads << " threads";
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
		acceptor.async_accept([this](auto err, auto socket) {
			if (!acceptor.is_open() || stopped) {
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
		});
	}

	asio::io_service io;
	asio::signal_set signals;
	asio::ip::tcp::acceptor acceptor;
	std::vector<std::thread> run_pool;
	std::atomic_bool stopped = false;
	Router router;
};
