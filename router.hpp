#pragma once

#include <cstdint>
#include <functional>
#include <regex>
#include <utility>
#include <vector>

class Connection;

class Router
{
public:
	enum Methods {
		UNKNOWN = 0x00,
		GET     = 0x01,
		POST    = 0x02,
		HEAD    = 0x04,
		OPTIONS = 0x08,
	};

	void add_route(std::string const& route, uint8_t methods, std::function<void(std::smatch const&, Methods, Connection&)> handler) {
		std::regex route_regex(route, std::regex::optimize);
		routes.emplace_back(route_regex, methods, handler);
	}

	bool handle_route(std::string const& route, std::string const& method_name, Connection& con) const {
		auto method = method_from_name(method_name);
		for (auto& r : routes) {
			std::smatch parts;
			if ((method & std::get<1>(r)) && std::regex_match(route, parts, std::get<0>(r))) {
				std::get<2>(r)(parts, method, con);
				return true;
			}
		}
		return false;
	}

private:
	Methods method_from_name(std::string const& name) const {
		if (name == "GET") return GET;
		if (name == "POST") return POST;
		if (name == "HEAD") return HEAD;
		if (name == "OPTIONS") return OPTIONS;
		return UNKNOWN;
	}
	std::vector<std::tuple<std::regex, std::uint8_t, std::function<void(std::smatch const&, Methods, Connection&)>>> routes;
};
