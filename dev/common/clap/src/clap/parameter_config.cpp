#include "parameter_config.hpp"
#include <sstream>

namespace clap {
	ParameterConfig::ParameterConfig(const base::RawView& long_name): long_name_(long_name) {}

	ParameterConfig::ParameterConfig(char short_name): short_name_(short_name) {}

	ParameterConfig& ParameterConfig::long_name(const base::RawView& long_name) {
		long_name_ = base::StrId(long_name);
		return *this;
	}

	ParameterConfig& ParameterConfig::short_name(char short_name) {
		short_name_ = short_name;
		return *this;
	}

	ParameterConfig& ParameterConfig::description(const base::RawView& description) {
		description_ = base::StrId(description);
		return *this;
	}

	ParameterConfig& ParameterConfig::required(const base::RawView& value_name) {
		required_   = true;
		value_name_ = base::StrId(value_name);
		return *this;
	}

	ParameterConfig& ParameterConfig::with_value(
		const base::RawView& value_name, const base::RawView& default_value
	) {
		default_value_ = base::StrId(default_value);
		value_name_    = base::StrId(value_name);
		return *this;
	}

	const option<base::StrId>& ParameterConfig::get_long_name() const { return long_name_; }

	const option<char>& ParameterConfig::get_short_name() const { return short_name_; }

	const option<base::StrId>& ParameterConfig::get_description() const { return description_; }

	bool ParameterConfig::is_required() const { return required_; }

	bool ParameterConfig::has_default_value() const { return default_value_.has_value(); }

	const option<base::StrId>& ParameterConfig::get_default_value() const { return default_value_; }

	const option<base::StrId>& ParameterConfig::get_value_name() const { return value_name_; }

	bool ParameterConfig::requires_argument() const { return value_name_.has_value(); }

	std::string ParameterConfig::short_help_message() const {
		std::stringstream builder;
		if (!required_) builder << '[';
		if (short_name_.has_value()) builder << '-' << short_name_.value();
		if (long_name_.has_value() && !short_name_.has_value())
			builder << "--" << long_name_.value().str();
		if (value_name_.has_value()) builder << ' ' << value_name_.value().str();
		if (!required_) builder << ']';
		return builder.str();
	}

	std::string ParameterConfig::long_help_message() const {
		std::stringstream builder;
		builder << "\t";
		if (short_name_.has_value()) builder << '-' << short_name_.value();
		if (long_name_.has_value()) {
			if (short_name_.has_value()) builder << ", ";
			builder << "--" << long_name_.value().str();
		}
		if (value_name_.has_value()) {
			builder << " " << value_name_.value().str();
			if (default_value_.has_value()) builder << "[= " << default_value_.value().str() << "]";
		}
		if (description_.has_value()) {
			if (long_name_.has_value() || value_name_.has_value()) builder << "\n\t";
			builder << "\t" << description_.value().str();
		}

		return builder.str();
	}

	base::StrId ParameterConfig::to_str_id() const {
		auto x = get_long_name();
		if (x.has_value()) return x.value();
		return base::StrId("" + get_short_name().value());
	}
}
