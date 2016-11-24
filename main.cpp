#include "server.hpp"

void home(std::smatch const& path, Router::Methods, Connection& con) {
	con.set_response("Hi from " + path[0].str());
}

int main()
{
	Server s;
	s.get_router().add_route("/", Router::GET, home);
	s.get_router().add_route("/home", Router::GET | Router::POST, home);
	s.get_router().add_route("/home2", Router::POST, home);

	s.run();

	return 0;
}
