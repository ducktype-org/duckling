#pragma once

#include <json/json.hpp>
#include <set>
#include <helios/symbols/symbols.hpp>
#include "source_position.hpp"

namespace dia {
	using nlohmann::json;

	class InteractiveCode {
	private:
		dia::SourcePosition               position;
		std::set<compiler::helios::SymID> symbols;

	public:
		InteractiveCode(dia::SourcePosition position): position(position) {}

		friend void to_json(json& j, const InteractiveCode& code) {
			j = json{ { "location",
				        code.position } };  // TODO: Get the source code and all of the symbols.
		}

		std::set<compiler::helios::SymID> get_symbols() const { return symbols; }
	};
}
