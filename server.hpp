#pragma once

#include <iostream>
#include <vector>
#include <thread>

#include "asio.hpp"

#include "connection.hpp"
#include "router.hpp"

namespace bb {
	class Server
	{
	public:
		Server(unsigned short port = 0) : signals(io), acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
			signals.add(SIGINT);
			signals.add(SIGTERM);
#if defined(SIGQUIT)
			signals.add(SIGQUIT);
#endif // defined(SIGQUIT)
			signals.async_wait([this](auto, auto) {
				std::cerr << "stopping... ";
				acceptor.close();
			});

			do_accept();
		}

		void run(unsigned int num_threads = (std::thread::hardware_concurrency() - 1)) {
			if (num_threads == 0)
				num_threads = 1;
			std::cerr << "Starting server on " << acceptor.local_endpoint().address() << ":" << acceptor.local_endpoint().port() << " using " << num_threads << " threads\n";
			for (unsigned int i = 0; i < num_threads; ++i) {
				run_pool.emplace_back([this]() { io.run(); });
			}
			for (unsigned int i = 0; i < num_threads; ++i) {
				run_pool[i].join();
			}
		}

		auto address() const { return acceptor.local_endpoint().address(); }
		auto port()    const { return acceptor.local_endpoint().port(); }

		template<typename ... T>
		auto add_route(T&& ... all) {
			return router.add_route(std::forward<T>(all)...);
		}

	private:
		void do_accept() {
			acceptor.async_accept([this](auto err, auto socket) {
				if (!err) {
					Connection::new_connection(std::move(socket), router)->start();
					do_accept();
				}
				else {
					if (err.value() == asio::error::operation_aborted) {
						std::cerr << "Stopped\n";
					}
					else {
						std::cerr << err.message() << '\n';
					}
				}
			});
		}

		asio::io_context io;
		asio::signal_set signals;
		asio::ip::tcp::acceptor acceptor;
		std::vector<std::thread> run_pool;
		Router router;
	}; // class Server
} // namespace bb
