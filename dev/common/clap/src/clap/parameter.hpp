/**
 * @file parameter.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief This class stores all the information about one parameter.
 */

#pragma once

#include <variant>

#include <base/optional.hpp>
#include <base/raw_view.hpp>

#include "value_parser.hpp"

namespace clap {

	// Forward declaration
	class ParsingResult;

	struct Optional final {};

	struct Required final {};

	struct Conditional final {
		using Condition = std::function<bool(const ParsingResult&)>;
		Condition   condition;
		std::string condition_description;
	};

	using ParameterNecessity = std::variant<Optional, Required, Conditional>;

	/**
	 * Parameter class is used to store all the information about the parameter/flag inside
	 * clap::Clap.
	 */
	class Parameter {
		friend class ParamBuilder;

	public:
		/**
		 * @return Optional short name of the parameter.
		 */
		[[nodiscard]]
		const base::Optional<char>& getShortName() const;
		/**
		 * @return Optional long name of the parameter.
		 */
		[[nodiscard]]
		const base::Optional<base::RawView>& getLongName() const;
		/**
		 * @return Short description of the parameter. Every parameter has a short description.
		 */
		[[nodiscard]]
		const base::RawView& getShortDesc() const;
		/**
		 * @return Optional long description of the parameter.
		 */
		[[nodiscard]]
		const base::Optional<base::RawView>& getLongDesc() const;
		/**
		 * @return Value parser pointer, that may be null.
		 */
		[[nodiscard]]
		MCRef<ValueParser> getValueParser() const;
		/**
		 * @return The necessity of a parameter.
		 */
		[[nodiscard]]
		const ParameterNecessity& getParameterNecessity() const;

	private:
		Parameter() = default;
		base::Optional<char>          short_name;
		base::Optional<base::RawView> long_name;
		base::RawView                 short_description;
		base::Optional<base::RawView> long_description;

		// If a ConfigParameter has a ValueParser, then
		// it means it's not a flag.
		MBox<ValueParser> value_parser;

		ParameterNecessity parameter_necessity;
	};
}
