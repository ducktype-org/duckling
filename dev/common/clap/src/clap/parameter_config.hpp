#pragma once

#include <base/optional.hpp>
#include <base/string_id.hpp>

namespace clap {
	class ParameterConfig {
	private:
		base::Optional<base::StrId> long_name_;
		base::Optional<char>        short_name_;
		base::Optional<base::StrId> description_;

		bool                        required_ = false;
		base::Optional<base::StrId> default_value_;
		base::Optional<base::StrId> value_name_;

	public:
		ParameterConfig(const base::RawView& long_name);
		ParameterConfig(char short_name);

		ParameterConfig& long_name(const base::RawView& long_name);
		ParameterConfig& short_name(char short_name);
		ParameterConfig& description(const base::RawView& description);
		ParameterConfig& required(const base::RawView& value_name);
		ParameterConfig&
			with_value(const base::RawView& value_name, const base::RawView& default_value);

		const base::Optional<base::StrId>& get_long_name() const;
		const base::Optional<char>&        get_short_name() const;
		const base::Optional<base::StrId>& get_description() const;
		bool                               is_required() const;
		bool                               has_default_value() const;
		const base::Optional<base::StrId>& get_default_value() const;
		const base::Optional<base::StrId>& get_value_name() const;
		bool                               requires_argument() const;

		std::string short_help_message() const;
		std::string long_help_message() const;

		base::StrId to_str_id() const;
	};
}
