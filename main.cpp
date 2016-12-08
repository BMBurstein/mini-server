#include "server.hpp"

void home(std::smatch const& path, Router::Methods method, Connection::ptr con) {
	con->send_response(200, "", "Hi from " + path[0].str());
}

int main()
{
	Server s(8080);
	s.get_router().add_route("/", Router::Methods::GET, home);
	s.get_router().add_route("/home", Router::Methods::GET | Router::Methods::POST, home);
	s.get_router().add_route("/home2", Router::Methods::POST, home);

	s.run(1);

	return 0;
}
