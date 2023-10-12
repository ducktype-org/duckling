#pragma once

#include <any>
#include <unordered_set>
#include <base/raw_view.hpp>
#include <base/maps.hpp>

namespace config {
	struct BadOptionAccess: base::LogicError {
	public:
		using base::LogicError::LogicError;
	};

	struct ParsingResult {
	private:
		base::HashMap<base::RawView, base::RawView> name_to_raw_value;
		base::HashMap<base::RawView, std::any>      name_to_value;
		std::unordered_set<base::RawView>           option_was;
		std::unordered_set<base::RawView>           all_options;
		std::vector<base::RawView>                  non_option_values;

		friend class ArgParser;

	public:
		bool          wasOption(base::RawView name) const;
		base::RawView getRawValue(base::RawView name);

		template<typename T>
		T getValue(base::RawView name) const {
			if (!wasOption(name)) {
				throw BadOptionAccess(base::strConcat("Requested option `", name, "` was not found")
				);
			}
			if (!name_to_raw_value.contains(name)) {
				throw BadOptionAccess(
					base::strConcat("Requested value of option `", name, "` was not found")
				);
			}
			return std::any_cast<T>(name_to_value[name]);
		}

		const std::vector<base::RawView>& getNonOptionValues() const { return non_option_values; }
	};
}
