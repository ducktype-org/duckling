#include "function_pass.hpp"

#include "const_eval.hpp"
#include "expr_lowering.hpp"
#include "type_name_extractor.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/if.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/while.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/keyword_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>

#include <base/except/exceptions.hpp>

#include <lang_definitions/key_spec_op.hpp>

namespace pg {
	void FunctionPass::visitFun(pst::Access<pst::Fun> stmt) {
		sym::SymbolID fun_sym_id = ctx.lookupInScopeAndParents(
			ctx.currNs().id, stmt->getName().illegalAccess().value()->unwrap()
		);
		sym::Symbol& fun_sym = ctx.getSymbol(fun_sym_id);

		const auto& fn_kind = std::get<sym::FunctionSymbol>(fun_sym.kind);
		const auto& fn_type = std::get<tp::Function>(ctx.getTypeInfo(fun_sym.type_id).kind);
		current_return_type = fn_type.return_type;

		llvm::Function* fn = lowering.getOrCreateFunction(fun_sym_id, ctx);

		llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
			auto* entry = llvm::BasicBlock::Create(llvm_ctx, "entry", fn);
			builder_stack.emplace_back(entry);
			locals.clear();

			// Enter the function's primary namespace so identifier lookups inside the body
			// hit local declarations (variables, parameters) before falling back to outer scopes.
			ctx.enterNamespace(fn_kind.primary_namespace);

			// Insert locals for parameters and spill the incoming argument values into them,
			// so parameters behave like ordinary mutable variables inside the body.
			for (auto [i, arg_sym_id]: std::views::enumerate(fn_kind.argument_symbols)) {
				const sym::Symbol& arg_sym = ctx.getSymbol(arg_sym_id);
				llvm::Type*        llvm_ty = lowering.lowerType(arg_sym.type_id, ctx);
				llvm::AllocaInst*  slot
					= builder_stack.back().CreateAlloca(llvm_ty, nullptr, arg_sym.name.strView());
				builder_stack.back().CreateStore(fn->getArg(static_cast<u32>(i)), slot);
				locals.insertOrAssign(arg_sym_id, slot);
			}

			pst::Access<pst::CodeBlockOrStmt> body = stmt->getBody().illegalAccess().value();
			for (pst::AccessLocked<pst::Stmt> inner_stmt:
			     *body->getCodeBlock().illegalAccess().value()) {
				inner_stmt.illegalAccess().value()->acceptVisitor(*this);
			}

			// Implicit function end: void functions return, non-void fallthrough is UB
			// (mark unreachable so the block is still terminated for the verifier).
			if (builder_stack.back().GetInsertBlock()->getTerminator() == nullptr) {
				if (fn_type.return_type == ctx.void_type_id)
					builder_stack.back().CreateRetVoid();
				else
					builder_stack.back().CreateUnreachable();
			}

			ctx.popNamespace();
			builder_stack.pop_back();
		});
	}

	void FunctionPass::visitVariable(pst::Access<pst::Variable> stmt) {
		tp::TypeID type_id = ctx.i64_type_id;
		if (stmt->getType().has_value()) {
			const pst::ExprElement& type_expr
				= *stmt->getType()->illegalAccess().value()->getExpr().illegalAccess().value();
			base::StrID type_name = extractTypeName(type_expr);
			if (type_name == base::StrID()) {
				throw std::runtime_error(
					"Variable '" + stmt->getName().illegalAccess().value()->unwrap().str()
					+ "' has a type expression of kind '" + type_expr.elementType()
					+ "' which is not a bare identifier (not yet supported)"
				);
			}
			type_id = ctx.lookupTypeInScopeAndParents(ctx.currNs().id, type_name);
		}

		sym::SymbolID sym_id = ctx.insertSymbol(
			stmt->getName().illegalAccess().value()->unwrap(), sym::VariableSymbol{}, type_id
		);

		llvm::Type*       llvm_ty = lowering.lowerType(type_id, ctx);
		llvm::AllocaInst* slot    = builder_stack.back().CreateAlloca(
            llvm_ty, nullptr, stmt->getName().illegalAccess().value()->unwrap().strView()
        );
		locals.insertOrAssign(sym_id, slot);

		// Optional initializer: var x: T = <expr>;
		if (stmt->getValue().has_value()) {
			const pst::ExprElement& init_expr
				= *stmt->getValue()->illegalAccess().value()->getExpr().illegalAccess().value();
			ExprLowering expr_lowering(ctx, lowering, builder_stack.back(), locals);
			LoweredExpr  init = expr_lowering.lower(init_expr);
			if (init.type_id != type_id) {
				throw std::runtime_error(
					"Variable '" + stmt->getName().illegalAccess().value()->unwrap().str()
					+ "': initializer type '" + ctx.getTypeInfo(init.type_id).name.str()
					+ "' does not match declared type '" + ctx.getTypeInfo(type_id).name.str() + "'"
				);
			}
			builder_stack.back().CreateStore(init.value, slot);
		}
	}

	void FunctionPass::visitExprStmt(pst::Access<pst::ExprStmt> stmt) {
		const pst::ExprElement& expr
			= *stmt->getExpr().illegalAccess().value()->getExpr().illegalAccess().value();
		ExprLowering expr_lowering(ctx, lowering, builder_stack.back(), locals);
		expr_lowering.lower(expr);
	}

	void FunctionPass::visitIf(pst::Access<pst::If> stmt) {
		auto& builder = builder_stack.back();

		// Lower the condition in the current block.
		const pst::ExprElement& cond_expr
			= *stmt->getCondition().illegalAccess().value()->getExpr().illegalAccess().value();
		ExprLowering expr_lowering(ctx, lowering, builder, locals);
		LoweredExpr  cond = expr_lowering.lower(cond_expr);
		if (cond.type_id != ctx.bool_type_id)
			throw std::runtime_error("If condition must be of type bool");

		llvm::Function* fn = builder.GetInsertBlock()->getParent();

		llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
			auto* then_block  = llvm::BasicBlock::Create(llvm_ctx, "if.then", fn);
			auto* else_block  = llvm::BasicBlock::Create(llvm_ctx, "if.else", fn);
			auto* merge_block = llvm::BasicBlock::Create(llvm_ctx, "if.merge", fn);

			builder.CreateCondBr(cond.value, then_block, else_block);

			// if true
			builder.SetInsertPoint(then_block);
			pst::Access<pst::CodeBlockOrStmt> then_body
				= stmt->getThenBody().illegalAccess().value();
			for (pst::AccessLocked<pst::Stmt> inner_stmt:
			     *then_body->getCodeBlock().illegalAccess().value()) {
				inner_stmt.illegalAccess().value()->acceptVisitor(*this);
			}
			bool if_true_fell_through = builder.GetInsertBlock()->getTerminator() == nullptr;
			if (if_true_fell_through) builder.CreateBr(merge_block);

			// if false
			builder.SetInsertPoint(else_block);
			if (stmt->getElseBody().has_value()) {
				pst::Access<pst::CodeBlockOrStmt> else_body
					= stmt->getElseBody()->illegalAccess().value();
				for (pst::AccessLocked<pst::Stmt> inner_stmt:
				     *else_body->getCodeBlock().illegalAccess().value()) {
					inner_stmt.illegalAccess().value()->acceptVisitor(*this);
				}
			}
			bool if_false_fell_through = builder.GetInsertBlock()->getTerminator() == nullptr;
			if (if_false_fell_through) builder.CreateBr(merge_block);

			// Get into the merge block.
			if (if_true_fell_through || if_false_fell_through) {
				builder.SetInsertPoint(merge_block);
			} else {
				// Both branches finished with a return, merge is unreachable. Drop it so LLVM
				// doesn't complain about a block without a terminator.
				merge_block->eraseFromParent();
			}
		});
	}

	void FunctionPass::visitWhile(pst::Access<pst::While> stmt) {
		auto&           builder = builder_stack.back();
		llvm::Function* fn      = builder.GetInsertBlock()->getParent();

		llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module&) {
			auto* cond_block  = llvm::BasicBlock::Create(llvm_ctx, "while.cond", fn);
			auto* body_block  = llvm::BasicBlock::Create(llvm_ctx, "while.body", fn);
			auto* after_block = llvm::BasicBlock::Create(llvm_ctx, "while.end", fn);

			// Get into the condition block.
			builder.CreateBr(cond_block);

			// Condition block.
			builder.SetInsertPoint(cond_block);
			const pst::ExprElement& cond_expr
				= *stmt->getCondition().illegalAccess().value()->getExpr().illegalAccess().value();
			ExprLowering expr_lowering(ctx, lowering, builder, locals);
			LoweredExpr  cond = expr_lowering.lower(cond_expr);
			if (cond.type_id != ctx.bool_type_id)
				throw std::runtime_error("While condition must be of type bool");
			builder.CreateCondBr(cond.value, body_block, after_block);

			// Body
			builder.SetInsertPoint(body_block);
			pst::Access<pst::CodeBlockOrStmt> body = stmt->getBody().illegalAccess().value();
			for (pst::AccessLocked<pst::Stmt> inner_stmt:
			     *body->getCodeBlock().illegalAccess().value()) {
				inner_stmt.illegalAccess().value()->acceptVisitor(*this);
			}
			// Back to condition (unless the body terminated, with `return`).
			if (builder.GetInsertBlock()->getTerminator() == nullptr) builder.CreateBr(cond_block);

			// Go to the after block.
			builder.SetInsertPoint(after_block);
		});
	}

	void FunctionPass::visitReturn(pst::Access<pst::Return> stmt) {
		if (!stmt->getValue().has_value()) {
			if (current_return_type != ctx.void_type_id)
				throw std::runtime_error("Return without a value in a non-void function");
			builder_stack.back().CreateRetVoid();
			return;
		}

		const pst::ExprElement& expr
			= *stmt->getValue()->illegalAccess().value()->getExpr().illegalAccess().value();

		ExprLowering expr_lowering(ctx, lowering, builder_stack.back(), locals);
		LoweredExpr  lowered = expr_lowering.lower(expr);

		if (lowered.type_id != current_return_type) {
			throw std::runtime_error(
				"Return value type '" + ctx.getTypeInfo(lowered.type_id).name.str()
				+ "' does not match function return type '"
				+ ctx.getTypeInfo(current_return_type).name.str() + "'"
			);
		}

		builder_stack.back().CreateRet(lowered.value);
	}

	void FunctionPass::visitNamespace(pst::Access<pst::Namespace> stmt) {
		// GlobalPass already created the namespace; re-enter it so the functions inside
		// are lowered in the right scope.
		const base::StrID name = stmt->getName().illegalAccess().value()->unwrap();
		sym::Symbol& ns_sym    = ctx.getSymbol(ctx.lookupInScopeAndParents(ctx.currNs().id, name));
		if (!std::holds_alternative<sym::NamespaceSymbol>(ns_sym.kind))
			throw std::runtime_error("'" + name.str() + "' is not a namespace");

		ctx.enterNamespace(std::get<sym::NamespaceSymbol>(ns_sym.kind).namespace_id);
		for (pst::AccessLocked<pst::LangElement> child: *stmt->getBody().illegalAccess().value())
			child.illegalAccess().value()->acceptVisitor(*this);
		ctx.popNamespace();
	}

	void FunctionPass::visitConst(pst::Access<pst::Const> stmt) {
		// Top-level consts (including those in namespaces) were already registered by
		// GlobalPass; only consts local to a function body are handled here.
		if (builder_stack.empty()) return;
		registerConstSymbol(ctx, stmt);
	}
}
