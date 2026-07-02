#pragma once

#include "const_eval.hpp"
#include "context.hpp"
#include "llvm.hpp"
#include "module_loader.hpp"
#include "type_name_extractor.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/numeric_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <psackling/ids.hpp>
#include <psackling/type.hpp>

#include <string_id/string_id.hpp>

namespace pg {
	class GlobalPass final: public pst::PstVisitorPanicky {
	public:
		GlobalPass(pg::Context& ctx): ctx(ctx) {}

	private:
		Context& ctx;

		void visitNamespace(pst::Access<pst::Namespace> stmt) override {
			ctx.pushNamespace(stmt->getName().illegalAccess().value()->unwrap());
			GlobalPass nested_pass(ctx);
			for (pst::AccessLocked<pst::LangElement> child: *stmt->getBody().illegalAccess().value())
				child.illegalAccess().value()->acceptVisitor(nested_pass);
			ctx.popNamespace();
		}

		void visitImport(pst::Access<pst::Import> stmt) override {
			const ImportSpec spec = parseImportSpec(*stmt);
			// Modules are processed dependency-first, so the imported module's
			// namespace chain already exists.
			const ns::NamespaceID target = ctx.resolveModulePath(spec.path);
			if (spec.star) {
				ctx.importNamespaceSymbols(target);
				return;
			}
			// `import math;` from the root file: the module's namespace symbol is
			// already visible under that exact name — nothing to alias.
			const base::StrID alias = spec.alias.value();
			if (ctx.currNs().symbols.contains(alias)) {
				sym::Symbol& existing = ctx.getSymbol(ctx.currNs().symbols[alias]);
				if (std::holds_alternative<sym::NamespaceSymbol>(existing.kind)
				    && std::get<sym::NamespaceSymbol>(existing.kind).namespace_id == target)
					return;
			}
			ctx.insertSymbol(
				alias, sym::NamespaceSymbol{ .namespace_id = target }, ctx.namespace_type_id
			);
		}

		/// Resolves a type expression (bare identifier or type keyword) to a TypeID,
		/// looked up from the current scope outwards.
		tp::TypeID resolveTypeExpr(const pst::ExprElement& type_expr, const std::string& what) {
			const base::StrID type_name = extractTypeName(type_expr);
			if (type_name == base::StrID()) {
				throw std::runtime_error(
					what + ": type expression of kind '" + type_expr.elementType()
					+ "' is not a bare identifier (not yet supported)"
				);
			}
			return ctx.lookupTypeInScopeAndParents(ctx.currNs().id, type_name);
		}

		void visitFun(pst::Access<pst::Fun> stmt) override {
			const base::StrID name = stmt->getName().illegalAccess().value()->unwrap();

			std::vector<tp::TypeID>    param_type_ids;
			std::vector<sym::SymbolID> argument_symbol_ids;
			// We are pushing an empty namespace for the function body
			auto func_ns_id = ctx.pushNamespace(base::StrID("$fn_" + name.str()));
			for (pst::AccessLocked<pst::Param> param: *stmt->getParams().illegalAccess().value()) {
				auto       param_accessed = param.illegalAccess().value();
				tp::TypeID param_type_id  = resolveTypeExpr(
                    *param_accessed->getType().illegalAccess().value()->getExpr().illegalAccess().value(
                    ),
                    "Parameter '" + param_accessed->getName().illegalAccess().value()->unwrap().str()
                        + "' of function '" + name.str() + "'"
                );
				sym::SymbolID arg_sym_id = ctx.insertSymbol(
					param_accessed->getName().illegalAccess().value()->unwrap(),
					sym::VariableSymbol{},
					param_type_id
				);
				param_type_ids.push_back(param_type_id);
				argument_symbol_ids.push_back(arg_sym_id);
			}
			ctx.popNamespace();

			tp::TypeID return_type_id = ctx.void_type_id;
			if (stmt->getRet().has_value()) {
				return_type_id = resolveTypeExpr(
					*stmt->getRet()->illegalAccess().value()->getExpr().illegalAccess().value(),
					"Return type of function '" + name.str() + "'"
				);
			}
			auto fun_type_id = ctx.newType(
				base::StrID(base::strConcat("$tp_", name.str())),
				tp::Function{ .param_types = std::move(param_type_ids),
			                  .return_type = return_type_id }
			);
			ctx.insertSymbol(
				name,
				sym::FunctionSymbol{ .primary_namespace = func_ns_id,
			                         .argument_symbols  = std::move(argument_symbol_ids),
			                         .link_name         = ctx.qualifyName(name) },
				fun_type_id
			);
		}

		void visitConst(pst::Access<pst::Const> stmt) override { registerConstSymbol(ctx, stmt); }

		void visitVariable(pst::Access<pst::Variable> stmt) override {
			const base::StrID name = stmt->getName().illegalAccess().value()->unwrap();

			// Globals are zero-initialized in the globals object file; initializer
			// expressions (which may call functions and read other globals) are lowered
			// into a synthetic constructor that runs before main, in declaration order.
			tp::TypeID type_id = ctx.i64_type_id;
			if (stmt->getType().has_value()) {
				type_id = resolveTypeExpr(
					*stmt->getType()->illegalAccess().value()->getExpr().illegalAccess().value(),
					"Global variable '" + name.str() + "'"
				);
			}

			sym::SymbolID sym_id = ctx.insertSymbol(
				name, sym::GlobalVariableSymbol{ .link_name = ctx.qualifyName(name) }, type_id
			);
			ctx.global_variables.push_back(sym_id);
		}

		void visitClass(pst::Access<pst::Class> stmt) override {
			const base::StrID name = stmt->getName().illegalAccess().value()->unwrap();

			// Create the classes namespace for the fields to be inserted.
			const ns::NamespaceID class_ns_id
				= ctx.pushNamespace(base::StrID("$cls_" + name.str()));

			std::vector<sym::SymbolID> field_symbols;
			u32                        index = 0;

			pst::Access<pst::CodeBlock> body = stmt->getBody().illegalAccess().value();

			for (pst::AccessLocked<pst::Stmt> member_locked: *body) {
				pst::Access<pst::Stmt> member = member_locked.illegalAccess().value();

				auto var_opt = member.dynamicCast<pst::Field>();
				if (!var_opt.has_value()) {
					throw std::runtime_error(
                    "Class '" + name.str()
                    + "': only field declarations are supported in the body for now "
                      "(unsupported member kind: "
                    + member->elementType() + ")"
                );
				}
				auto field = var_opt.value();

				const base::StrID field_name = field->getName().illegalAccess().value()->unwrap();

				if (!field->getType().illegalAccess().has_value()) {
					throw std::runtime_error(
						"Class field '" + name.str() + "." + field_name.str()
						+ "' must declare an explicit type"
					);
				}
				const pst::ExprElement& type_expr
					= *field->getType().illegalAccess().value()->getExpr().illegalAccess().value();
				const base::StrID type_name = extractTypeName(type_expr);
				if (type_name == base::StrID()) {
					throw std::runtime_error(
						"Class field '" + name.str() + "." + field_name.str()
						+ "': type expression of kind '" + type_expr.elementType()
						+ "' is not a bare identifier (not yet supported)"
					);
				}

				// Field type must have been previously declared.
				tp::TypeID field_type_id
					= ctx.lookupTypeInScopeAndParents(ctx.currNs().id, type_name);

				sym::SymbolID field_sym_id = ctx.insertSymbol(
					field_name, sym::FieldSymbol{ .index = index }, field_type_id
				);
				field_symbols.push_back(field_sym_id);
				++index;

				// Initializers on class fields are ignored. Zero-initialized by default.
			}

			ctx.popNamespace();

			// Build the class type class_ns_id, but don't let newType insert the
			// TypeSymbol that would land it in the class's own namespace instead of outside.
			tp::TypeID type_id = ctx.newTypeInScope(
				name,
				tp::Structure{ .structure_symbol_id = sym::SymbolID{},
			                   .field_symbols       = std::move(field_symbols),
			                   .qualified_name      = ctx.qualifyName(name) },
				class_ns_id
			);

			// Now expose the class name in the outer scope so `var p: Point;` resolves.
			sym::SymbolID class_sym_id
				= ctx.insertSymbol(name, sym::TypeSymbol{ .type_id = type_id }, type_id);

			auto& kind = ctx.getTypeInfo(type_id).kind;
			std::get<tp::Structure>(kind).structure_symbol_id
				= class_sym_id;  // Now fill the SymbolID
		}
	};
}
