#include "function_pass.hpp"

#include "expr_lowering.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/keyword_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>

#include <base/except/exceptions.hpp>
#include <lang_definitions/key_spec_op.hpp>

namespace {
	/// Tiny visitor that extracts a type name from a PST type expression
	/// (currently only bare identifiers like `i64`/`bool` are supported).
	/// Uses the Empty base so non-identifier nodes are silently ignored — the caller
	/// inspects the resulting `name` and decides whether extraction succeeded.
	class TypeNameExtractor final: public pst::expr::PstExprVisitorEmpty {
	public:
		base::StrID name{};

		void visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> ident) override {
			name = ident->getName().value;
		}

		// Built-in primitive type names (`i64`, `bool`, `void`, ...) are tokenized as keywords.
		void visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> kw) override {
			name = lang_def::keywordToStr(kw->getKeyword());
		}
	};

	base::StrID extractTypeName(const pst::ExprElement& expr) {
		TypeNameExtractor extractor;
		expr.acceptExprVisitor(extractor);
		return extractor.name;
	}
}

namespace pg {
	void FunctionPass::visitFun(pst::Access<pst::Fun> stmt) {
		sym::SymbolID fun_sym_id = ctx.lookupInScopeAndParents(ctx.currNs().id, stmt->getName());
		sym::Symbol&  fun_sym    = ctx.getSymbol(fun_sym_id);

		llvm::Type* lowered = lowering.lowerType(fun_sym.type_id, ctx);
		auto*       fn_ty   = llvm::cast<llvm::FunctionType>(lowered);

		const auto& fn_kind = std::get<sym::FunctionSymbol>(fun_sym.kind);

		llvm_module.inCtxDo([&](llvm::LLVMContext& llvm_ctx, llvm::Module& mod) {
			llvm::Function* fn = llvm::Function::Create(
				fn_ty, llvm::Function::ExternalLinkage, stmt->getName().strView(), &mod
			);
			auto* entry = llvm::BasicBlock::Create(llvm_ctx, "entry", fn);
			builder_stack.emplace_back(entry);
			locals.clear();

			// Enter the function's primary namespace so identifier lookups inside the body
			// hit local declarations (variables, parameters) before falling back to outer scopes.
			ctx.enterNamespace(fn_kind.primary_namespace);

			pst::Access<pst::CodeBlockOrStmt> body = stmt->getBody().illegalAccess().value();
			for (pst::AccessLocked<pst::Stmt> inner_stmt:
			     *body->getCodeBlock().illegalAccess().value()) {
				inner_stmt.illegalAccess().value()->acceptVisitor(*this);
			}

			ctx.popNamespace();
			builder_stack.pop_back();
		});
	}

	void FunctionPass::visitVariable(pst::Access<pst::Variable> stmt) {
		tp::TypeID type_id = ctx.i64_type_id;
		if (stmt->getType().has_value()) {
			const pst::ExprElement& type_expr = *stmt->getType()
			                                          ->illegalAccess()
			                                          .value()
			                                          ->getExpr()
			                                          .illegalAccess()
			                                          .value();
			base::StrID type_name = extractTypeName(type_expr);
			if (type_name == base::StrID()) {
				throw std::runtime_error(
					"Variable '" + stmt->getName().str()
					+ "' has a type expression of kind '" + type_expr.elementType()
					+ "' which is not a bare identifier (not yet supported)"
				);
			}
			type_id = ctx.lookupTypeInScopeAndParents(ctx.currNs().id, type_name);
		}

		sym::SymbolID sym_id
			= ctx.insertSymbol(stmt->getName(), sym::VariableSymbol{}, type_id);

		llvm::Type* llvm_ty = lowering.lowerType(type_id, ctx);
		llvm::AllocaInst* slot
			= builder_stack.back().CreateAlloca(llvm_ty, nullptr, stmt->getName().strView());
		locals.insertOrAssign(sym_id, slot);

		// Optional initializer: var x: T = <expr>;
		if (stmt->getValue().has_value()) {
			const pst::ExprElement& init_expr = *stmt->getValue()
			                                          ->illegalAccess()
			                                          .value()
			                                          ->getExpr()
			                                          .illegalAccess()
			                                          .value();
			ExprLowering expr_lowering(ctx, lowering, builder_stack.back(), locals);
			LoweredExpr  init = expr_lowering.lower(init_expr);
			builder_stack.back().CreateStore(init.value, slot);
		}
	}

	void FunctionPass::visitReturn(pst::Access<pst::Return> stmt) {
		const pst::ExprElement& expr = *stmt->getValue()
		                                          ->illegalAccess()
		                                          .value()
		                                          ->getExpr()
		                                          .illegalAccess()
		                                          .value();

		ExprLowering expr_lowering(ctx, lowering, builder_stack.back(), locals);
		LoweredExpr  lowered = expr_lowering.lower(expr);

		builder_stack.back().CreateRet(lowered.value);
	}
}
