/**
 * @file config_parameter.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "base/raw_view.hpp"
#include "value_parser.hpp"
#include "base/optional.hpp"
#include "parsing_result.hpp"
#include <variant>

namespace clap {

	struct Optional {};

	struct Required {};

	struct Conditional {
		using Condition = std::function<bool(ParsingResult)>;
		Condition condition;
	};

	using ParameterNecessity = std::variant<Optional, Required, Conditional>;

	class ConfigParameter {
		friend class ParamBuilder;

	public:
		[[nodiscard]]
		const base::Optional<base::RawView>& getShortName() const;
		[[nodiscard]]
		const base::Optional<base::RawView>& getLongName() const;
		[[nodiscard]]
		const base::RawView& getShortDesc() const;
		[[nodiscard]]
		const base::Optional<base::RawView>& getLongDesc() const;
		[[nodiscard]]
		const ValueParser* getValueParser() const;
		[[nodiscard]]
		const ParameterNecessity& getParameterNecessity() const;

	private:
		ConfigParameter() = default;
		base::Optional<base::RawView> short_name;
		base::Optional<base::RawView> long_name;
		base::RawView                 short_description;  // Required short description.
		base::Optional<base::RawView> long_description;

		// If a ConfigParameter has a ValueParser, then
		// it means it's not a flag.
		// Using pointer here, because of polymorphism.
		base::unique_ptr<ValueParser> value_parser;

		ParameterNecessity parameter_necessity;
	};
}
