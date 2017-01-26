#include "server.hpp"

using namespace bb;

void home(std::smatch const& path, Router::Methods method, Connection::ptr con) {
	con->send_response(200, "", "Hi from " + path[0].str());
}

int main()
{
	Server s(8080);
	s.add_route("/", Router::Methods::GET, home);
	s.add_route("/home", Router::Methods::GET | Router::Methods::POST, home);
	s.add_route("/home2", Router::Methods::POST, home);

	s.run();

	return 0;
}
