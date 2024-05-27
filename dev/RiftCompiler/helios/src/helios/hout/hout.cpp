#include "hout.hpp"

namespace compiler::helios {

	std::string HOUTUnit::debugPrint() const {
		std::string out;

		out += "HOUT UNIT:\n";
		out += "Functions:\n";
		for (auto& func: functions) {
			out += func.debugPrint();
		}

		return out;
	}

	std::string HOUTFunction::debugPrint() const {
		std::string out;
		// @TODO

		out += "fun ";
		out += original_name.strView();
		out += " ( @TODO ) -> @TODO {\n";
		out += "    @TODO\n";
		out += "}\n";
		return out;
	}


}

