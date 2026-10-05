// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file parameter.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief This class stores all the information about one parameter.
 */

#pragma once

#include "value_parser.hpp"

#include <base/collections/optional.hpp>
#include <base/misc/raw_view.hpp>

#include <variant>

namespace clah {

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
	 * clah::Clah.
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

		/**
		 * Returns a name of the parameter.
		 * A long name if available, short name otherwise or an empty optional is none is specified.
		 * @return  Optional name of the parameter.
		 */
		[[nodiscard]] base::Optional<std::string> getParameterName() const;

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
