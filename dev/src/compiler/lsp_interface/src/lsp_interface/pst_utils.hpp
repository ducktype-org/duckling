// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/access/access_pst.hpp>
#include <frontend/pst_parser/elements/hierarchy/meta.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <diagnostic/source_position.hpp>

namespace duck_ls {

	/**
	 * @brief Whether `position`, the range a node takes in the file, covers `offset`. The end
	 * of a source position is the offset of its last character.
	 */
	bool covers(const dia::SourcePosition& position, usize offset);

	/**
	 * @brief The two places under the cursor that a hover can describe, found on the way
	 * from the root of the PST down to the offset.
	 */
	struct PstPath final {
		/// The outermost expression containing the offset inside the innermost statement that
		/// contains it, like `n + 2i64` in `var b = { var y = n + 2i64; y };` when the cursor
		/// is on `n`. HELIOS gives HOUT only for whole expressions like this one, not for
		/// their parts, so the hover asks for this one and then looks inside it. The two sides
		/// of an assignment count as separate expressions.
		base::Optional<pst::Access<pst::ExprElement>> top_expression{};
		/// The innermost declaration whose name contains the offset, like `var sum` when the
		/// cursor is on `sum`.
		base::Optional<pst::Access<pst::Stmt>> declaration{};
	};

	/**
	 * @brief Walks the PST of `file` from the root down to the smallest element containing
	 * `offset`, and returns what it found on the way.
	 *
	 * At each level the walk goes into the one child that contains `offset`, and stops when no
	 * child does. It records the first expression it passes as `PstPath::top_expression` and
	 * the last declaration whose name contains `offset` as `PstPath::declaration`.
	 *
	 * The PST is read with `illegalAccess`, which is safe here because this runs outside any
	 * query: it only picks what to ask HELIOS about later.
	 *
	 * @param offset A code point offset, as `toCodePointOffset` computes it.
	 * @return Both fields empty when the file has parse errors, because HELIOS only runs on code
	 * that parses.
	 */
	PstPath getPstPath(base::Ref<compiler::frontend::SourceFile> file, usize offset);
}
