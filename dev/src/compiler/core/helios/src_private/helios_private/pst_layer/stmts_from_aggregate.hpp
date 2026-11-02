// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file stmts_from_aggregate.hpp
 * @brief Functions that perform some walks over PST
 */

#pragma once

#include <frontend/pst_parser/elements/includes/basic.hpp>  // for pst::Stmt

#include <vector>

namespace compiler::helios {
	template<std::derived_from<pst::Stmt> Stmt = pst::Stmt>
	using StmtList = std::vector<pst::AccessLocked<Stmt>>;

	/**
	 * @brief Returns all children statements of given LangElement
	 * Currently:
	 *  * For CodeBlock return Stmt in the code block
	 *  * For CodeBlockOrStmt return Stmt in the code block
	 *  * For TopLevel return top level Stmt in the PST
	 *  * For ClassBlock return all children statements of the class block
	 *    Including onces in access blocks
	 *  * For other it panics
	 *
	 * @note It expands and handles macros.
	 *
	 * @return StmtList
	 */
	StmtList<> getStmtsFromStmtAggregate(query::Context&, pst::AccessLocked<pst::LangElement>);
}
