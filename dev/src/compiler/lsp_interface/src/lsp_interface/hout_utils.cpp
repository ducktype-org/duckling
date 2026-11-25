// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <helios/hout/origin.hpp>
#include <helios/utils/hout_walker_generic.hpp>
#include <lsp_interface/hout_utils.hpp>
#include <lsp_interface/pst_utils.hpp>

#include <query_framework/context/context.hpp>

#include <concepts>
#include <type_traits>

namespace duck_ls {

	namespace code = compiler::helios::code;

    base::Optional<CRef<code::Expr>> findInnermostExpression(
        query::Context& ctx, const code::Expr& root, usize offset, const dia::SourcePosition& bounds
    ) {
        base::Optional<CRef<code::Expr>> found{};
        // How long `found` is (end minus start), so that each new candidate can be compared
        // with it without asking HELIOS for its position again.
        usize found_size = 0;

        auto consider = [&](const auto& node) {
            using Node = std::remove_cvref_t<decltype(node)>;
            // The walker hands over statements too; only an expression can be the answer. A
			// reusable expression only wraps a written one, like the `n` of `1 < n < 5`, that
			// HELIOS uses twice; the walk goes into its first use, where the written one is.
			if constexpr (std::derived_from<Node, code::Expr>
			              && !std::same_as<Node, code::ReusableExpr>) {
                const code::Expr& expr = node;
                if (expr.origin.isGenerated()) return;

                auto position_opt = expr.origin.getSourcePosition(ctx);
                if (!position_opt.has_value()) return;
                const auto& position = position_opt.value();

                if (position.getSource().get() != bounds.getSource().get()) return;
                if (position.getStart() < bounds.getStart() || position.getEnd() > bounds.getEnd())
                    return;
                if (!covers(position, offset)) return;

                // Children come after their parents, so on a tie the deeper one wins.
                const usize size = position.getEnd() - position.getStart();
                if (found.empty() || size <= found_size) {
                    found      = &expr;
                    found_size = size;
                }
            }
        };

        code::walkExprTree(root, consider);
        return found;
    }

}