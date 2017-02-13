#include <thread>

#define ASIO_STANDALONE 1
#define ASIO_NO_DEPRECATED 1

#include "server.hpp"
#include "client_connection.hpp"

using namespace bb;

void home(std::smatch const& path, Methods method, Connection::ptr con) {
	con->send_response(200, "", "Hi from home() on " + path[0].str() + '\n');
}

struct Response
{
	void operator()(std::smatch const& path, Methods method, Connection::ptr con) {
		con->send_response(200, "", "Hi from Response on " + path[0].str() + '\n');
	}
};

int main()
{
	Server s(8080);
	Response responder;
	s.add_route("/", Methods::GET, responder);
	s.add_route("/home", Methods::GET | Methods::POST, home);
	s.add_route("/home2", Methods::POST, responder);
	s.add_route("/fwd/([^/:]+)(:([0-9]+))?(/.*)", Methods::GET, [&](std::smatch const& path, Methods method, Connection::ptr con) {
		try {
			std::string port = path[3];
			ClientConnection::new_connection(s.context(), path[1], (!port.empty() ? port : "http"))->send_request(path[4], "", [con{ std::move(con) }](ClientConnection::ptr ptr) {
				con->send_response(ptr->status(), "", "");
			});
		}
		catch (std::error_code const& ec) {
			std::cout << ec.message();
		}
	});

	s.run(std::thread::hardware_concurrency());

	return 0;
}
