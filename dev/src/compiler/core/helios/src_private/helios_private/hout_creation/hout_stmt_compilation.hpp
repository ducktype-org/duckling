// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/context/context.hpp>

#include <memory>

namespace compiler::helios {

	/**
	 * @brief Compile a PST function/method body into a shared HOUT CodeBlock.
	 *
	 * Processes all statements within the function body using HoutStmtMaker
	 * and returns the resulting HOUT CodeBlock. The return_type parameter specifies
	 * the expected return type of the enclosing function, which is used to validate
	 * and coerce return statements.
	 *
	 * @note This function is specifically intended for top-level function/method bodies.
	 * Nested control-flow bodies (e.g. if/else/while bodies) are lowered recursively
	 * as part of their parent statement compilation and should NOT be compiled via
	 * direct calls to this function.
	 *
	 * @param return_type The return type of the enclosing function, needed for return
	 *                    statement type validation and coercion.
	 *
	 * @note Returns a shared_ptr to match HOUTFunction::body storage and
	 * constructor API (`std::shared_ptr<const code::CodeBlock>`).
	 */
	std::shared_ptr<const code::CodeBlock> compileCodeOfCodeBlock(
		query::Context&                         ctx,
		pst::AccessLocked<pst::CodeBlockOrStmt> container,
		tsh::SymbolType<>                       return_type
	);

	/**
	 * @brief Overload for a bare `pst::CodeBlock` container.
	 *
	 * Used for function-like bodies that are always a code block, like the class destructor.
	 */
	std::shared_ptr<const code::CodeBlock> compileCodeOfCodeBlock(
		query::Context&                   ctx,
		pst::AccessLocked<pst::CodeBlock> container,
		tsh::SymbolType<>                 return_type
	);

	/**
	 * @brief Compile a single PST statement into a HOUT CodeBlock.
	 *
	 * Uses HoutStmtMaker internally to convert the given PST statement (e.g. if/while/for/block)
	 * into its HOUT representation and wraps it in a CodeBlock.
	 * Intended for use by QueryReplInstructionWrapper to wrap a bare REPL instruction in a
	 * synthetic void function.
	 *
	 * This helper compiles exactly one statement entry point. Nested blocks inside
	 * that statement are lowered recursively by the statement compiler internals.
	 *
	 * @param ctx        The active query context
	 * @param stmt       The PST statement to compile
	 * @param return_type The enclosing return-type context (typically void for REPL instructions)
	 * @return CodeBlock containing the compiled statement
	 */
	code::CodeBlock compileSingleStatement(
		query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt, tsh::SymbolType<> return_type
	);

}
