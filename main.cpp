#include "server.hpp"
#include "objmgr.hpp"

void home(std::smatch const& path, Router::Methods method, Connection::ptr con) {
	con->send_response(200, "", "Hi from " + path[0].str());
}

class C {
public:
	C(int, int) {};
	C(C&&) = default;
	C(C const&) = default;
};
int main()
{
	Object_Manager<C> om;
	om.push(1,2);
	
	Server s;
	s.get_router().add_route("/", Router::Methods::GET, home);
	s.get_router().add_route("/home", Router::Methods::GET | Router::Methods::POST, home);
	s.get_router().add_route("/home2", Router::Methods::POST, home);

	s.run();

	return 0;
}
