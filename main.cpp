#include <thread>

#include "server.hpp"

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

	s.run(std::thread::hardware_concurrency());

	return 0;
}
