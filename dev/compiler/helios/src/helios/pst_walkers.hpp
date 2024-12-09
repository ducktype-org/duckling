/**
 * @file pst_walkers.hpp
 * @brief Functions that perform some walks over PST
 * @TODO: decide if this should be in some separate module.
 */

#pragma once

#include <pst_parser/elements/elements.hpp>
#include <pst_parser/lang_parser_state.hpp>
#include <vector>

namespace compiler::helios {
	// @future: walkers for class and other stuff

	template<std::derived_from<pst::Stmt> Stmt = pst::Stmt>
	using StmtList = std::vector<MCRef<Stmt>>;

	/**
	 * @brief Returns all children statements of given LangElement
	 * Currently:
	 *  * For CodeBlock return Stmt in the code block
	 *  * For CodeBlockOrStmt return Stmt in the code block
	 *  * For TopLevel return top level Stmt in the PST
	 *  * For other it panics
	 *
	 * @return StmtList
	 */
	StmtList<> getStmtsFromStmtAggregate(MCRef<pst::LangElement>);

	/**
	 * @brief Returns all children statements of given ClassBlock
	 * Flattens access specifier blocks as their information is included in statements.
	 *
	 * @return StmtList
	 */
	StmtList<pst::ClassStmt> getChildStmtsOfClassBlock(MCRef<pst::LangElement>);
}
