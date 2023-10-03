#pragma once

#include <base/option.hpp>
#include <base/string_id.hpp>

namespace clap {
	class ParameterConfig {
	private:
		option<base::StrId> long_name_   = none<base::StrId>();
		option<char>        short_name_  = none<char>();
		option<base::StrId> description_ = none<base::StrId>();

		bool                required_      = false;
		option<base::StrId> default_value_ = none<base::StrId>();
		option<base::StrId> value_name_    = none<base::StrId>();

	public:
		ParameterConfig(const base::RawView& long_name);
		ParameterConfig(char short_name);

		ParameterConfig& long_name(const base::RawView& long_name);
		ParameterConfig& short_name(char short_name);
		ParameterConfig& description(const base::RawView& description);
		ParameterConfig& required(const base::RawView& value_name);
		ParameterConfig&
			with_value(const base::RawView& value_name, const base::RawView& default_value);

		const option<base::StrId>& get_long_name() const;
		const option<char>&        get_short_name() const;
		const option<base::StrId>& get_description() const;
		bool                       is_required() const;
		bool                       has_default_value() const;
		const option<base::StrId>& get_default_value() const;
		const option<base::StrId>& get_value_name() const;
		bool                       requires_argument() const;

		std::string short_help_message() const;
		std::string long_help_message() const;

		base::StrId to_str_id() const;
	};
}  // namespace clap
