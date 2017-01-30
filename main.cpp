#include <thread>

#include "server.hpp"

using namespace bb;

void home(std::smatch const& path, Router::Methods method, Connection::ptr con) {
	con->send_response(200, "", "Hi from home() on " + path[0].str());
}

struct Response
{
	void operator()(std::smatch const& path, Router::Methods method, Connection::ptr con) {
		con->send_response(200, "", "Hi from Response on " + path[0].str());
	}
};

int main()
{
	Server s(8080);
	Response responder;
	s.add_route("/", Router::Methods::GET, responder);
	s.add_route("/home", Router::Methods::GET | Router::Methods::POST, home);
	s.add_route("/home2", Router::Methods::POST, responder);

	s.run(std::thread::hardware_concurrency());

	return 0;
}
