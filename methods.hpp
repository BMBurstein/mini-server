#pragma once

#include <string>

#include "bitmask.hpp"

namespace bb {
	enum class Methods {
		GET = 0x01,
		POST = 0x02,
		HEAD = 0x04,
		OPTIONS = 0x08,
		UNKNOWN = 0x80,
		ANY = 0xFF,
	};

	ENABLE_BITMASK_OPERATORS(Methods);

	static Methods method_from_name(std::string const& name) {
		if (name == "GET") return Methods::GET;
		if (name == "POST") return Methods::POST;
		if (name == "HEAD") return Methods::HEAD;
		if (name == "OPTIONS") return Methods::OPTIONS;
		return Methods::UNKNOWN;
	}

	static std::string name_from_method(Methods method) {
		switch (method)
		{
		case Methods::GET: return "GET";
		case Methods::POST: return "POST";
		case Methods::HEAD: return "HEAD";
		case Methods::OPTIONS: return "OPTIONS";
		default: return "UNKNOWN";
		}
	}
}
