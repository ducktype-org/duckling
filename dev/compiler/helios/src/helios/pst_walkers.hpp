/**
 * @file pst_walkers.hpp
 * @brief Functions that perform some walks over PST
 * @TODO: decide if this should be in some separate module.
 */

#pragma once

#include <base/ref.hpp>
#include <pst_parser/elements/elements.hpp>  // for pst::Stmt
#include <vector>

namespace compiler::helios {
	template<std::derived_from<pst::Stmt> Stmt = pst::Stmt>
	using StmtList = std::vector<MCRef<Stmt>>;

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
	 * @return StmtList
	 */
	StmtList<> getStmtsFromStmtAggregate(MCRef<pst::LangElement>);
}
