// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/elements/expr.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <diagnostic/source_position.hpp>
#include <query_framework/context/context_fd.hpp>

namespace duck_ls {

    /**
    * @brief The smallest HOUT expression in `root` whose source position covers `offset`,
    * or empty when there is none.
    *
    * HELIOS also adds expressions that are not written in the code, such as an implicit cast
    * or dereference, or a default argument copied into a call. Two rules keep them out:
    * - An added expression is never chosen itself, but the walk still goes into its children,
    *   so hovering over `y` in an implicit cast of `y` shows `y`, not the cast.
    * - An expression counts only if its position is in the same file and inside `bounds`,
    *   the range of `root`. A copied default argument keeps the position of the declaration
    *   it came from, which can be anywhere.
    */
	base::Optional<CRef<compiler::helios::code::Expr>> findInnermostExpression(
		query::Context&                     ctx,
		const compiler::helios::code::Expr& root,
		usize                               offset,
		const dia::SourcePosition&          bounds
	);

}