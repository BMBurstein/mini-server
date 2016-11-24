#include "server.hpp"
//#include "connection.hpp"



// The '\r' at the end is needed since getline only strips the '\n'
const std::regex Connection::re_req(R"(([A-Z]+) ([-/%.+?=&\w]+) HTTP/1.[10]\r)", std::regex::optimize);
const std::regex Connection::re_head(R"(([-\w]+): (.*)\r)", std::regex::optimize);



int main()
{
	Server s;
	s.run();

	return 0;
}
