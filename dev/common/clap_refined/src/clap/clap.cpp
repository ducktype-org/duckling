/**
 * @file config_parameter.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "clap.hpp"

namespace clap {
	Clap& Clap::add(ConfigParameter&& parameter) {
		if (parameter.getValueParser() != nullptr) {
			if (started_keyword_args) throw base::LogicError("Cannot add positional after keyword");
			started_keyword_args = true;
		}
		parameters.push_back(std::move(parameter));
		return *this;
	}

	ParsingResult Clap::parse(usize argc, char* const* argv) {
		std::string arg0 = std::string(argv[0]);
		std::string args;
		for (usize i = 1; i < argc; i++) args += std::string(argv[i]) + " ";


	}

	const ValueParser* Clap::getDefaultValueParser() const { return default_value_parser.get(); }

	const std::vector<ConfigParameter>& Clap::getParameters() const { return parameters; }
}  // clap
