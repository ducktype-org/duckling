// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/collections/maps.hpp>

#include <diagnostic/message.hpp>
#include <query_framework/context/context.hpp>

namespace compiler::helios {
	/**
	 * @brief This element adds the alias chain information to the message
	 * in case of types that are aliases.
	 * Is also responsible for displaying the type name in the intuitive way,
	 * i.e. the same way the user wrote it in the source code.
	 */
	class InteractiveType: public dia::InteractiveElement {
		tsh::SymbolType<>                                 symbol_type;
		base::Optional<pst::Access<pst::LangElement>>     pst_expr;
		base::HashMap<std::string, Box<dia::MessageBase>> linked_messages;
		std::string                                       displayed_name;

		Box<dia::dia_args::Component> getValue(dia::MessageBase& msg) final;

	public:
		InteractiveType(
			query::Context&                               ctx,
			tsh::SymbolType<>                             symbol_type,
			base::Optional<pst::Access<pst::LangElement>> pst_expr = {}
		);
	};

	/**
	 * @brief This element adds the function declaration attachment to the message
	 * in case of function and methods.
	 * Is also responsible for displaying the function name in the intuitive way,
	 * i.e. the same way the user wrote it in the source code.
	 * It will support methods as well in the future.
	 */
	class InteractiveFunction final: public dia::InteractiveElement {
		SymID                                             function_symbol;
		base::Optional<pst::Access<pst::LangElement>>     pst_expr;
		base::HashMap<std::string, Box<dia::MessageBase>> linked_messages;
		std::string                                       displayed_name;

		Box<dia::dia_args::Component> getValue(dia::MessageBase& msg) final;

	public:
		InteractiveFunction(
			query::Context&                               ctx,
			SymID                                         function_symbol,
			base::Optional<pst::Access<pst::LangElement>> pst_expr = {}
		);
	};
}
