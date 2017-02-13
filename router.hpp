#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <regex>
#include <utility>
#include <vector>

#include "methods.hpp"

namespace bb {
	class Connection;

	class Router
	{
	private:
		typedef std::shared_ptr<Connection> ConnectionPtr;
	
	public:
		typedef std::function<void(std::smatch const&, Methods, ConnectionPtr)> HandlerFunc;

		void add_route(std::string const& route, Methods methods, HandlerFunc handler) {
			std::regex route_regex(route, std::regex::optimize);
			routes.emplace_back(route_regex, methods, handler);
		}

		bool handle_route(std::string const& route, std::string const& method_name, ConnectionPtr con) const {
			auto method = method_from_name(method_name);
			for (auto& r : routes) {
				std::smatch parts;
				if (((method & std::get<1>(r)) == method) && std::regex_match(route, parts, std::get<0>(r))) {
					std::get<2>(r)(parts, method, std::move(con));
					return true;
				}
			}
			return false;
		}

	private:
		std::vector<std::tuple<std::regex, Methods, HandlerFunc>> routes;
	}; // class Router
} // namespace bb
