// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file param_builder.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief This is the class through which clah::Parameter should be instantiated.
 */

#pragma once

#include "parameter.hpp"
#include "value_parser.hpp"

#include <utility>

namespace clah {

	/**
	 * This exception is not meant to be handled, as it means clah::Parameter
	 * has been built inappropriately.
	 */
	struct ParamBuilderException: std::exception {
		std::string what_str;

	public:
		explicit ParamBuilderException(std::string what_str): what_str(std::move(what_str)) {}

		[[nodiscard]]
		const char* what() const noexcept override {
			return what_str.c_str();
		}
	};

	/**
	 * This class is provided as the one and only way to build clah::Parameter.
	 */
	class ParamBuilder {
	public:
		/**
		 * @name Factories
		 * Methods to instantiate the class.
		 * @{
		 */
		/**
		 * Creates a new ParamBuilder object to build a named parameter that parses value with
		 * value_parser.
		 * @param value_parser Value parser to be used to parse the value of the parameter.
		 * @return A new object of class ParamBuilder.
		 */
		static ParamBuilder ofValue(Box<ValueParser> value_parser);
		/**
		 * Creates a new ParamBuilder object to build a flag.
		 * @return A new object of class ParamBuilder.
		 */
		static ParamBuilder ofFlag();
		/**@}*/

		/**
		 * Add a short name to the parameter.
		 * @param new_short_name The name as a char.
		 * @return A reference to self for the builder design pattern.
		 */
		ParamBuilder& addShortName(char new_short_name);
		/**
		 * Add a long name to the parameter in a form of a string.
		 * @param new_long_name The name as std::string.
		 * @return A reference to self for the builder design pattern.
		 */
		ParamBuilder& addLongName(base::RawView new_long_name);
		/**
		 * Add a short description to the parameter, that is displayed i.e. in a help page.
		 * Usually takes a form of a single sentence.
		 * @param new_short_desc The description.
		 * @return A reference to self for the builder design pattern.
		 */
		ParamBuilder& addShortDesc(base::RawView new_short_desc);
		/**
		 * Add a long description to the parameter, that is displayed i.e. in a help page.
		 * Usually takes a form of multiple sentences.
		 * @param new_long_desc The description.
		 * @return A reference to self for the builder design pattern.
		 */
		ParamBuilder& addLongDesc(base::RawView new_long_desc);

		/**
		 * Make the parameter optional - this is a default.
		 * Optional means that the parameter does not have to be specified
		 * in the command-line arguments.
		 * @return A reference to self for the builder design pattern.
		 */
		ParamBuilder& optional();

		/**
		 * Make the parameter required.
		 * Required means that the parameter has to always be specified
		 * in the command-line arguments.
		 * @return A reference to self for the builder design pattern.
		 */
		ParamBuilder& required();
		/**
		 * Make the parameter conditional.
		 * Conditional means that it is conditionally required or conditionally forbidden
		 * under certain conditions.
		 * @param condition Function that returns true if parameter is correct - passes the
		 * condition, false if not. Function takes a parameter clah::ParsingResult to deduce if it's
		 * correct or not.
		 * @param description Description of the condition.
		 * @return A reference to self for the builder design pattern.
		 */
		ParamBuilder& conditional(
			Conditional::Condition&& condition, const std::string& description = ""
		);

		/**
		 * Builds the parameter. May throw exception if parameter wasn't provided with enough
		 * information.
		 * @return the built parameter.
		 */
		Parameter build();

	private:
		ParamBuilder() { optional(); }

		base::Optional<char>          short_name;
		base::Optional<base::RawView> long_name;
		base::Optional<base::RawView> short_description;
		base::Optional<base::RawView> long_description;

		MBox<ValueParser> value_parser;

		base::Optional<ParameterNecessity> parameter_necessity;
	};
}
