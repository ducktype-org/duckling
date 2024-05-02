/**
 * @file pst_walkers.hpp
 * @brief Functions that perform some walks over PST 
 * @TODO: decide if this should be in some separate module.
 */

#pragma once

#include <pst_parser/elements/elements.hpp>
#include <pst_parser/rift_parser_base.hpp>
#include <vector>

#include "pst_ref.hpp"

namespace compiler::helios {
	// @future: walkers for class and other stuff

	using StmtList = std::vector<PstRef<pst::Stmt>>;

	/**
	 * @brief Returns all child statements of given RiftElement
	 * Currently:
	 *  * For CodeBlock return Stmt in the code block
	 *  * For CodeBlockOrStmt return Stmt in the code block
	 *  * For TopLevel return top level Stmt in the PST
	 *  * For other it panics
	 * 
	 * @return StmtList 
	 */
	StmtList getChildStmtsOf(PstRef<pst::RiftElement>);

}
