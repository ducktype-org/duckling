/**
 * @file clap_parameter.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "base/raw_view.hpp"
#include "value_parser.hpp"
#include "base/optional.hpp"
#include <variant>

namespace clap {

	class ParsingResult;  // Forward declaration

	struct Optional {};

	struct Required {};

	struct Conditional {
		using Condition = std::function<bool(ParsingResult)>;
		Condition condition;
	};

	using ParameterNecessity = std::variant<Optional, Required, Conditional>;

	class ClapParameter {
		friend class ParamBuilder;

	public:
		[[nodiscard]]
		const base::Optional<char>& getShortName() const;
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
		ClapParameter() = default;
		base::Optional<char>          short_name;
		base::Optional<base::RawView> long_name;
		base::RawView                 short_description;
		base::Optional<base::RawView> long_description;

		// If a ConfigParameter has a ValueParser, then
		// it means it's not a flag.
		// Using pointer here, because of polymorphism.
		base::unique_ptr<ValueParser> value_parser;

		ParameterNecessity parameter_necessity;
	};
}
