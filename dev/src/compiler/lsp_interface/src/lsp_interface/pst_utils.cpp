// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/elements/hierarchy/meta.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/wrapper_elements/identifier_wrapper.hpp> 
#include <frontend/pst_parser/base_element/lang_parser_element.hpp>
#include <frontend/pst_parser/pst/parsed_pst.hpp>
#include <lsp_interface/pst_utils.hpp>

namespace duck_ls {

	bool covers(const dia::SourcePosition& position, usize offset) {
		return position.getStart() <= offset && offset <= position.getEnd();
	}

	namespace {
		/**
		 * @brief Whether `statement` declares a symbol whose name covers `offset`.
		 */
		bool declaresNameAt(pst::Access<pst::Stmt> statement, usize offset) {
			if (statement->isDeclaration() != pst::DeclKind::Symbol) return false;

			auto identifier_locked = statement->getDeclSymbolIdentifier();
			if (!identifier_locked.has_value()) return false;

			auto identifier = identifier_locked.value().illegalAccess();
			if (!identifier.has_value()) return false;

			return covers(identifier.value()->getSourcePosition().illegalAccess(), offset);
		}

		/**
		 * @brief Walks from `element` down to the smallest element containing `offset`, filling
		 * `path` on the way.
		 *
		 * At each level it goes into the one child that contains `offset`, and stops when no
		 * child does. On the way it records the first expression it passes after the last
		 * statement as `path.top_expression`, and the last declaration whose name contains
		 * `offset` as `path.declaration`.
		 */
		void descend(pst::Access<pst::LangElement> element, usize offset, PstPath& path) {
			if (auto statement = element.dynamicCast<pst::Stmt>()) {
				path.top_expression.reset();

				if (declaresNameAt(statement.value(), offset))
					path.declaration.emplace(statement.value());
			}

			if (path.top_expression.empty()
			    && element.dynamicCast<pst::expr::Assignment>().empty()) {
				if (auto expression = element.dynamicCast<pst::ExprElement>())
					path.top_expression.emplace(expression.value());
			}
	
			for (auto child_locked: element->viewChildren()) {
				auto child = child_locked.illegalAccess();
				if (!child.has_value()) continue;
				if (!covers(child.value()->getSourcePosition().illegalAccess(), offset)) continue;

				descend(child.value(), offset, path);
				return;
			}
		}
	}

	PstPath getPstPath(base::Ref<compiler::frontend::SourceFile> file, usize offset) {
		PstPath path = {};

		auto pst = file->getPST();
		if (pst->hasErrors()) return path;

		auto root = pst->getRootElement().illegalAccess();
		if (!root.has_value()) return path;

		descend(root.value(), offset, path);
		return path;
	}

}
