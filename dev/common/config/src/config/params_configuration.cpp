#include "params_configuration.hpp"

namespace config {
	OptionDescription::OptionDescription(base::RawView long_version, base::RawView description):
	description(description), long_version(long_version), short_version(""),
	has_param(false), has_short(false) {}

	OptionDescription::OptionDescription(base::RawView long_version, base::RawView short_version, base::RawView description):
		description(description), long_version(long_version), short_version(short_version),
		has_param(false), has_short(true) {}

	OptionDescription::OptionDescription(base::RawView long_version, ParamType param_typ, base::unique_ptr<ValueParser> value_parser, base::RawView description):
		description(description), long_version(long_version), short_version(""),
		param_type(param_typ), value_parser(std::move(value_parser)),
		has_param(true), has_short(false) {}

	OptionDescription::OptionDescription(base::RawView long_version, base::RawView short_version, ParamType param_typ, base::unique_ptr<ValueParser> value_parser, base::RawView description):
		description(description), long_version(long_version), short_version(short_version),
		param_type(param_typ), value_parser(std::move(value_parser)),
		has_param(true), has_short(true) {}


	void ConfigOptions::generateOptionDesc(printer::Message& in) {
		for (auto& opt: options) {
			if (opt.has_short) {
				in.add({"-", opt.short_version.stdString(), ", "});
			}
			in.add({"--", opt.long_version.stdString()});
			if (opt.has_param) {
				if (opt.param_type == ParamType::Always) {
					in.add(base::strConcat(" ", opt.value_parser->helperMess()));
				}
				else if (opt.param_type == ParamType::Optional) {
					in.add(base::strConcat(" [", opt.value_parser->helperMess(), "]"));
				}
			}
			// @TODO: allow printer to perform column alignment and other control sequences in message
			in.add({"\t\t ", opt.description.stdString(), "\n"});
		}
	}

}
