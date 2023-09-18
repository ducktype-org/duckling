#pragma once

#include "value_parser.hpp"

#include <base/raw_view.hpp>
#include <base/unique_pointer.hpp>
#include <printer/printer.hpp>
#include <vector>

namespace config {

	enum class ParamType { Optional, Always };

	struct OptionDescription {
		base::RawView                 description;
		base::RawView                 long_version;

		base::RawView                 short_version;

		ParamType                     param_type;
		base::unique_ptr<ValueParser> value_parser;

		bool                          has_param;
		bool                          has_short;

		OptionDescription(base::RawView long_version, base::RawView description);
		OptionDescription(base::RawView long_version, base::RawView short_version,
		                  base::RawView description);
		OptionDescription(base::RawView long_version, ParamType param_typ,
		                  base::unique_ptr<ValueParser> value_parser, base::RawView description);
		OptionDescription(base::RawView long_version, base::RawView short_version,
		                  ParamType param_typ, base::unique_ptr<ValueParser> value_parser,
		                  base::RawView description);
	};

	class ConfigOptions {
		std::vector<OptionDescription> options;
		friend class ArgParser;

	public:
		template<typename... Args>
		ConfigOptions& addOption(Args&&... args) {
			options.emplace_back(std::forward<Args>(args)...);
			return *this;
		}

		void generateOptionDesc(printer::Message& in);
	};

}
