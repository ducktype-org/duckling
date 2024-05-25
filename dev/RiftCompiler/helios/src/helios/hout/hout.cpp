#include "hout.hpp"

namespace compiler::helios {

	std::string HOUTUnit::debugPrint() const {
		std::string out;

		out += "HOUT UNIT:";
		out += "Functions:";
		for (auto& func: functions) {
			out += func.debugPrint();
		}

		return out;
	}

	std::string HOUTFunction::debugPrint() const {
		std::string out;
		// @TODO
		out += "@TODO\n";

		return out;
	}


}

