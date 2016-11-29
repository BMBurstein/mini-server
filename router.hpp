#pragma once

#include <cstdint>
#include <functional>
#include <regex>
#include <utility>
#include <vector>

#include "connection.hpp"
#include "bitmask.hpp"

class Router
{
public:
	enum class Methods {
		GET     = 0x01,
		POST    = 0x02,
		HEAD    = 0x04,
		OPTIONS = 0x08,
		UNKNOWN = 0x80,
		ANY     = 0xFF,
	};

	void add_route(std::string const& route, Methods methods, std::function<void(std::smatch const&, Methods, Connection::ptr)> handler) {
		std::regex route_regex(route, std::regex::optimize);
		routes.emplace_back(route_regex, methods, handler);
	}

	bool handle_route(std::string const& route, std::string const& method_name, Connection::ptr con) const;

private:
	Methods method_from_name(std::string const& name) const {
		if (name == "GET") return Methods::GET;
		if (name == "POST") return Methods::POST;
		if (name == "HEAD") return Methods::HEAD;
		if (name == "OPTIONS") return Methods::OPTIONS;
		return Methods::UNKNOWN;
	}
	std::vector<std::tuple<std::regex, Methods, std::function<void(std::smatch const&, Methods, Connection::ptr)>>> routes;
};

ENABLE_BITMASK_OPERATORS(Router::Methods);

inline bool Router::handle_route(std::string const& route, std::string const& method_name, Connection::ptr con) const {
	auto method = method_from_name(method_name);
	for (auto& r : routes) {
		std::smatch parts;
		if (((method & std::get<1>(r)) == method) && std::regex_match(route, parts, std::get<0>(r))) {
			std::get<2>(r)(parts, method, con);
			return true;
		}
	}
	return false;
}
