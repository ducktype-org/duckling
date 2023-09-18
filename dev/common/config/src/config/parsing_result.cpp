#include "parsing_result.hpp"

namespace config {
	bool ParsingResult::wasOption(base::RawView name) const {
		if (!all_options.contains(name)) {
			throw BadOptionAccess("Option doesn't exist");
		}
		return option_was.contains(name);
	}

	base::RawView ParsingResult::getRawValue(base::RawView name) {
		if (!wasOption(name)) {
			throw BadOptionAccess(base::strConcat("Requested option `", name, "` was not found"));
		}
		if (!name_to_raw_value.contains(name)) {
			throw BadOptionAccess(
				base::strConcat("Requested value of option `", name, "` was not found"));
		}
		return name_to_raw_value[name];
	}
}
