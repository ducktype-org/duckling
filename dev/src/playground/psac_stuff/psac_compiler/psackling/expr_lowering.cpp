#include "expr_lowering.hpp"

#include "char_literal.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/binary_operator.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/call.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/chain_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/comparison_chain.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/keyword_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/numeric_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/round_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/pst.hpp>

#include <base/except/exceptions.hpp>

#include <lang_definitions/key_spec_op.hpp>

namespace pg {
	LoweredExpr ExprLowering::lower(const pst::ExprElement& expr) {
		result = {};
		expr.acceptExprVisitor(*this);
		return result;
	}

	void ExprLowering::visitExprNumericValue(pst::Access<pst::expr::ExprNumericValue> lit) {
		const u64   value   = std::stoull(lit->getValue().str());
		llvm::Type* llvm_ty = lowering.lowerType(ctx.i64_type_id, ctx);
		result              = LoweredExpr{
						 .value   = llvm::ConstantInt::get(llvm_ty, value),
						 .type_id = ctx.i64_type_id,
		};
	}

	void ExprLowering::visitExprCharValue(pst::Access<pst::expr::ExprCharValue> lit) {
		const char  value   = charLiteralValue(lit);
		llvm::Type* llvm_ty = lowering.lowerType(ctx.char_type_id, ctx);
		result              = LoweredExpr{
						 .value   = llvm::ConstantInt::get(llvm_ty, static_cast<unsigned char>(value)),
						 .type_id = ctx.char_type_id,
		};
	}

	void ExprLowering::visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> kw) {
		// In expression position only the boolean literals are meaningful for now.
		// Type-name keywords (`i64`, `bool`, ...) only appear in type position and are
		// extracted by FunctionPass before reaching the expression lowering.
		llvm::Type* i1_ty = lowering.lowerType(ctx.bool_type_id, ctx);
		switch (kw->getKeyword().illegalAccess().value()->unwrap()) {
		case lang_def::Keyword::True:
			result = LoweredExpr{
				.value   = llvm::ConstantInt::get(i1_ty, 1),
				.type_id = ctx.bool_type_id,
			};
			return;
		case lang_def::Keyword::False:
			result = LoweredExpr{
				.value   = llvm::ConstantInt::get(i1_ty, 0),
				.type_id = ctx.bool_type_id,
			};
			return;
		default:
			throw std::runtime_error(
				"Keyword '"
				+ lang_def::keywordToStr(kw->getKeyword().illegalAccess().value()->unwrap()).str()
				+ "' is not a valid value expression"
			);
		}
	}

	LoweredExpr ExprLowering::symbolRefToExpr(sym::SymbolID sym_id, base::StrID name) {
		sym::Symbol& symbol = ctx.getSymbol(sym_id);

		// Arrays are never loaded as whole values (a VLA has no static LLVM type at
		// all); the expression stays address-only and is consumed by `[...]` links.
		const bool is_array
			= std::holds_alternative<tp::Array>(ctx.getTypeInfo(symbol.type_id).kind);

		if (std::holds_alternative<sym::VariableSymbol>(symbol.kind)) {
			if (!locals.contains(sym_id)) {
				CORE_PANIC(
					"Variable '" + name.str() + "' has no associated stack slot (locals map miss)"
				);
			}
			llvm::AllocaInst* slot = locals[sym_id];
			if (is_array) return LoweredExpr{ .type_id = symbol.type_id, .address = slot };
			llvm::Type* llvm_ty = lowering.lowerType(symbol.type_id, ctx);
			return LoweredExpr{
				.value   = builder.CreateLoad(llvm_ty, slot, name.strView()),
				.type_id = symbol.type_id,
				.address = slot,
			};
		}

		if (std::holds_alternative<sym::GlobalVariableSymbol>(symbol.kind)) {
			llvm::GlobalVariable* global = lowering.getOrCreateGlobal(sym_id, ctx);
			if (is_array) return LoweredExpr{ .type_id = symbol.type_id, .address = global };
			llvm::Type* llvm_ty = lowering.lowerType(symbol.type_id, ctx);
			return LoweredExpr{
				.value   = builder.CreateLoad(llvm_ty, global, name.strView()),
				.type_id = symbol.type_id,
				.address = global,
			};
		}

		if (std::holds_alternative<sym::ConstantSymbol>(symbol.kind)) {
			ctx.notifyConstantUse(symbol);
			llvm::Type* llvm_ty = lowering.lowerType(symbol.type_id, ctx);
			return LoweredExpr{
				.value
				= llvm::ConstantInt::get(llvm_ty, std::get<sym::ConstantSymbol>(symbol.kind).bits),
				.type_id = symbol.type_id,
			};
		}

		if (std::holds_alternative<sym::FunctionSymbol>(symbol.kind)
		    || std::holds_alternative<sym::NamespaceSymbol>(symbol.kind)
		    || std::holds_alternative<sym::TypeSymbol>(symbol.kind)) {
			// No runtime value; a later chain link (call or `.` access) consumes it.
			// A TypeSymbol is consumed by a constructor call: `Point()`.
			return LoweredExpr{ .type_id = symbol.type_id, .symbol = sym_id };
		}

		CORE_PANIC(
			"Identifier '" + name.str() + "' names a symbol kind not supported in expressions"
		);
	}

	void ExprLowering::visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> ident) {
		const base::StrID name = ident->getName().illegalAccess().value()->unwrap();
		result = symbolRefToExpr(ctx.lookupInScopeAndParents(ctx.currNs().id, name), name);
	}

	void ExprLowering::visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> op) {
		LoweredExpr lhs = lower(*op->getLeftOperand().illegalAccess().value());
		LoweredExpr rhs = lower(*op->getRightOperand().illegalAccess().value());

		// Logical `and`/`or` come from PST as BinaryOperator subclasses (LogicAnd/LogicOr)
		// whose operator is a keyword (StrID "and"/"or") — asNamed() reports NotAnOperator,
		// so we match by raw operator StrID.
		const base::StrID op_str = op->getOperator().illegalAccess().value()->unwrap();
		if (op_str == lang_def::keywordToStr(lang_def::Keyword::And)
		    || op_str == lang_def::keywordToStr(lang_def::Keyword::Or)) {
			if (lhs.type_id != ctx.bool_type_id || rhs.type_id != ctx.bool_type_id)
				throw std::runtime_error("Logical '" + op_str.str() + "' requires bool operands");
			// No short-circuit yet; both sides are already evaluated above.
			llvm::Value* out = (op_str == lang_def::keywordToStr(lang_def::Keyword::And))
			                     ? builder.CreateAnd(lhs.value, rhs.value, "andtmp")
			                     : builder.CreateOr(lhs.value, rhs.value, "ortmp");
			result           = LoweredExpr{ .value = out, .type_id = ctx.bool_type_id };
			return;
		}

		// Comparisons accept i64 (signed order) and char (unsigned byte order) operand
		// pairs; arithmetic stays i64-only — chars don't participate, like in the real
		// compiler where char is not an integral type.
		const bool both_i64  = lhs.type_id == ctx.i64_type_id && rhs.type_id == ctx.i64_type_id;
		const bool both_char = lhs.type_id == ctx.char_type_id && rhs.type_id == ctx.char_type_id;
		if (!both_i64 && !both_char) {
			throw std::runtime_error(
				"Binary operator currently only supports i64 or char operand pairs"
			);
		}
		auto require_i64 = [&] {
			if (!both_i64)
				throw std::runtime_error(
					"Binary arithmetic operator currently only supports i64 operands"
				);
		};

		llvm::Value* out_value   = nullptr;
		tp::TypeID   result_type = ctx.i64_type_id;
		switch (op->getOperator().illegalAccess().value()->unwrap().asNamed()) {
		case lang_def::NamedOperator::Plus:
			require_i64();
			out_value = builder.CreateAdd(lhs.value, rhs.value, "addtmp");
			break;
		case lang_def::NamedOperator::Minus:
			require_i64();
			out_value = builder.CreateSub(lhs.value, rhs.value, "subtmp");
			break;
		case lang_def::NamedOperator::Multiply:
			require_i64();
			out_value = builder.CreateMul(lhs.value, rhs.value, "multmp");
			break;
		case lang_def::NamedOperator::Divide:
			// Signed division — i64 is signed in our type system.
			require_i64();
			out_value = builder.CreateSDiv(lhs.value, rhs.value, "divtmp");
			break;
		case lang_def::NamedOperator::Remainder:
			require_i64();
			out_value = builder.CreateSRem(lhs.value, rhs.value, "remtmp");
			break;
		case lang_def::NamedOperator::Equal:
			out_value   = builder.CreateICmpEQ(lhs.value, rhs.value, "eqtmp");
			result_type = ctx.bool_type_id;
			break;
		case lang_def::NamedOperator::NotEqual:
			out_value   = builder.CreateICmpNE(lhs.value, rhs.value, "netmp");
			result_type = ctx.bool_type_id;
			break;
		case lang_def::NamedOperator::Lesser:
			out_value   = both_char ? builder.CreateICmpULT(lhs.value, rhs.value, "lttmp")
			                        : builder.CreateICmpSLT(lhs.value, rhs.value, "lttmp");
			result_type = ctx.bool_type_id;
			break;
		case lang_def::NamedOperator::LEqual:
			out_value   = both_char ? builder.CreateICmpULE(lhs.value, rhs.value, "letmp")
			                        : builder.CreateICmpSLE(lhs.value, rhs.value, "letmp");
			result_type = ctx.bool_type_id;
			break;
		case lang_def::NamedOperator::Greater:
			out_value   = both_char ? builder.CreateICmpUGT(lhs.value, rhs.value, "gttmp")
			                        : builder.CreateICmpSGT(lhs.value, rhs.value, "gttmp");
			result_type = ctx.bool_type_id;
			break;
		case lang_def::NamedOperator::GEqual:
			out_value   = both_char ? builder.CreateICmpUGE(lhs.value, rhs.value, "getmp")
			                        : builder.CreateICmpSGE(lhs.value, rhs.value, "getmp");
			result_type = ctx.bool_type_id;
			break;
		default:
			throw std::runtime_error(
				"Binary operator '" + op->getOperator().illegalAccess().value()->unwrap().str()
				+ "' is not yet supported (+ - * /)"
			);
		}

		result = LoweredExpr{ .value = out_value, .type_id = result_type };
	}

	void ExprLowering::visitComparisonChain(pst::Access<pst::expr::ComparisonChain> chain) {
		// Chain `a OP1 b OP2 c` is lowered as `(a OP1 b) and (b OP2 c)`. Each link compares
		// adjacent values on i64 (only supported numeric type for now); result is bool.

		auto        expr_count = usize(chain->numberOfSubExpressions());
		const auto& ops        = std::views::iota(0ULL, expr_count - 1)
		                | std::views::transform([chain](usize i) { return chain->getOperator(i); });
		if (ops.empty())
			throw std::runtime_error("Empty comparison chain (PST invariant violation)");

		auto cmp_for
			= [&](lang_def::NamedOperator op_kind, llvm::Value* l, llvm::Value* r, bool is_char
		      ) -> llvm::Value* {
			switch (op_kind) {
			case lang_def::NamedOperator::Equal:
				return builder.CreateICmpEQ(l, r, "eqtmp");
			case lang_def::NamedOperator::NotEqual:
				return builder.CreateICmpNE(l, r, "netmp");
			case lang_def::NamedOperator::Lesser:
				return is_char ? builder.CreateICmpULT(l, r, "lttmp")
				               : builder.CreateICmpSLT(l, r, "lttmp");
			case lang_def::NamedOperator::LEqual:
				return is_char ? builder.CreateICmpULE(l, r, "letmp")
				               : builder.CreateICmpSLE(l, r, "letmp");
			case lang_def::NamedOperator::Greater:
				return is_char ? builder.CreateICmpUGT(l, r, "gttmp")
				               : builder.CreateICmpSGT(l, r, "gttmp");
			case lang_def::NamedOperator::GEqual:
				return is_char ? builder.CreateICmpUGE(l, r, "getmp")
				               : builder.CreateICmpSGE(l, r, "getmp");
			default:
				throw std::runtime_error(
					"Unsupported comparison operator in chain (== != < <= > >=)"
				);
			}
		};

		LoweredExpr  left     = lower(*chain->getSubExpr(0).illegalAccess().value());
		llvm::Value* combined = nullptr;

		for (u64 i = 0; i < ops.size(); ++i) {
			LoweredExpr right = lower(*chain->getSubExpr(i + 1).illegalAccess().value());

			const bool both_i64
				= left.type_id == ctx.i64_type_id && right.type_id == ctx.i64_type_id;
			const bool both_char
				= left.type_id == ctx.char_type_id && right.type_id == ctx.char_type_id;
			if (!both_i64 && !both_char)
				throw std::runtime_error(
					"Comparison currently only supports i64 or char operand pairs"
				);

			llvm::Value* link = cmp_for(
				ops[i].illegalAccess().value()->unwrap().asNamed(), left.value, right.value, both_char
			);
			combined = combined ? builder.CreateAnd(combined, link, "chaintmp") : link;
			left     = right;
		}

		result = LoweredExpr{ .value = combined, .type_id = ctx.bool_type_id };
	}

	void ExprLowering::visitRoundExpr(pst::Access<pst::expr::RoundExpr> round) {
		// Parentheses carry no semantic meaning; just lower the inner expression.
		result = lower(*round->getInner().illegalAccess().value());
	}

	LoweredExpr ExprLowering::emitIndex(const LoweredExpr& base, pst::Access<pst::expr::Call> call) {
		if (base.address == nullptr)
			throw std::runtime_error("Subscript on a non-l-value expression");
		const tp::TypeInfo& base_type = ctx.getTypeInfo(base.type_id);
		if (!std::holds_alternative<tp::Array>(base_type.kind))
			throw std::runtime_error("Subscript on non-array type '" + base_type.name.str() + "'");
		const auto& array = std::get<tp::Array>(base_type.kind);

		auto args = call->getArgs().illegalAccess().value();
		if (args->size() != 1) throw std::runtime_error("Subscript takes exactly one index");
		pst::Access<pst::CallArgument> arg = (*args->begin()).illegalAccess().value();
		if (arg->isNamedArg())
			throw std::runtime_error("Subscript index cannot be a named argument");

		LoweredExpr index
			= lower(*arg->getArg().illegalAccess().value()->getExpr().illegalAccess().value());
		if (index.type_id != ctx.i64_type_id)
			throw std::runtime_error("Subscript index must be of type i64");

		// GEP by element type off the first element: correct for fixed arrays
		// ([N x T] allocas and globals) and the only option for VLAs. No bounds
		// checking — C semantics.
		llvm::Type*  elem_ty  = lowering.lowerType(array.element, ctx);
		llvm::Value* elem_ptr = builder.CreateGEP(elem_ty, base.address, index.value, "arridx");

		// An element that is itself an array stays address-only, ready for the next
		// `[...]` link; scalar and struct elements load like any other l-value.
		if (std::holds_alternative<tp::Array>(ctx.getTypeInfo(array.element).kind))
			return LoweredExpr{ .type_id = array.element, .address = elem_ptr };
		return LoweredExpr{
			.value   = builder.CreateLoad(elem_ty, elem_ptr, "arrelem"),
			.type_id = array.element,
			.address = elem_ptr,
		};
	}

	LoweredExpr ExprLowering::emitCall(const LoweredExpr& callee, pst::Access<pst::expr::Call> call) {
		if (call->getType() == lexer::Token::BracketType::Square) return emitIndex(callee, call);
		if (call->getType() != lexer::Token::BracketType::Round)
			throw std::runtime_error("Subscript expressions are not yet supported");

		if (!callee.symbol.has_value()) throw std::runtime_error("Call target is not a function");
		const sym::SymbolID fun_sym_id = callee.symbol.value();
		sym::Symbol&        fun_sym    = ctx.getSymbol(fun_sym_id);

		// Constructor call `Point()`: produces a zero-initialized value of the type.
		// No user-defined constructors yet, so arguments are not allowed.
		if (std::holds_alternative<sym::TypeSymbol>(fun_sym.kind)) {
			if (call->getArgs().illegalAccess().value()->size() != 0) {
				throw std::runtime_error(
					"Constructor of '" + fun_sym.name.str()
					+ "' takes no arguments (only default construction is supported)"
				);
			}
			const tp::TypeID type_id = std::get<sym::TypeSymbol>(fun_sym.kind).type_id;
			llvm::Type*      llvm_ty = lowering.lowerType(type_id, ctx);
			return LoweredExpr{
				.value   = llvm::Constant::getNullValue(llvm_ty),
				.type_id = type_id,
			};
		}

		if (!std::holds_alternative<sym::FunctionSymbol>(fun_sym.kind))
			throw std::runtime_error("'" + fun_sym.name.str() + "' is not a function");

		const auto& fn_type = std::get<tp::Function>(ctx.getTypeInfo(fun_sym.type_id).kind);

		std::vector<llvm::Value*> args;
		for (pst::AccessLocked<pst::CallArgument> arg_locked:
		     *call->getArgs().illegalAccess().value()) {
			pst::Access<pst::CallArgument> arg = arg_locked.illegalAccess().value();
			if (arg->isNamedArg())
				throw std::runtime_error(
					"Named call arguments are not yet supported (call to '" + fun_sym.name.str()
					+ "')"
				);

			LoweredExpr lowered
				= lower(*arg->getArg().illegalAccess().value()->getExpr().illegalAccess().value());

			const usize index = args.size();
			if (index < fn_type.param_types.size()
			    && lowered.type_id != fn_type.param_types[index]) {
				throw std::runtime_error(
					"Call to '" + fun_sym.name.str() + "': argument " + std::to_string(index + 1)
					+ " has type '" + ctx.getTypeInfo(lowered.type_id).name.str() + "', expected '"
					+ ctx.getTypeInfo(fn_type.param_types[index]).name.str() + "'"
				);
			}
			args.push_back(lowered.value);
		}

		if (args.size() != fn_type.param_types.size()) {
			throw std::runtime_error(
				"Call to '" + fun_sym.name.str() + "': expected "
				+ std::to_string(fn_type.param_types.size()) + " arguments, got "
				+ std::to_string(args.size())
			);
		}

		llvm::Function* fn = lowering.getOrCreateFunction(fun_sym_id, ctx);
		auto*      fn_ty = llvm::cast<llvm::FunctionType>(lowering.lowerType(fun_sym.type_id, ctx));
		const bool is_void = fn_type.return_type == ctx.void_type_id;
		llvm::Value* ret   = builder.CreateCall(fn_ty, fn, args, is_void ? "" : "calltmp");

		return LoweredExpr{
			.value   = is_void ? nullptr : ret,
			.type_id = fn_type.return_type,
		};
	}

	void ExprLowering::visitCall(pst::Access<pst::expr::Call>) {
		// A Call only carries the argument list; the callee comes from the surrounding
		// chain expression, so a bare Call can't be lowered on its own.
		CORE_PANIC("Call expression outside of a chain (no callee available)");
	}

	void ExprLowering::visitAssignment(pst::Access<pst::expr::Assignment> assignment) {
		const base::StrID op = assignment->getAssignmentType().illegalAccess().value()->unwrap();

		LoweredExpr lhs = lower(*assignment->getVariables().illegalAccess().value());
		if (lhs.address == nullptr)
			throw std::runtime_error("Left-hand side of '" + op.str() + "' is not an l-value");
		if (std::holds_alternative<tp::Array>(ctx.getTypeInfo(lhs.type_id).kind))
			throw std::runtime_error("Arrays cannot be assigned as a whole");

		LoweredExpr rhs = lower(*assignment->getValue().illegalAccess().value());

		if (lhs.type_id != rhs.type_id) {
			const tp::TypeInfo& lhs_ti = ctx.getTypeInfo(lhs.type_id);
			const tp::TypeInfo& rhs_ti = ctx.getTypeInfo(rhs.type_id);
			throw std::runtime_error(
				"Type mismatch in '" + op.str() + "': lhs is '" + lhs_ti.name.str() + "', rhs is '"
				+ rhs_ti.name.str() + "'"
			);
		}

		llvm::Value* value_to_store = nullptr;

		if (op == base::StrID("=")) {
			value_to_store = rhs.value;
		} else {
			// Compound assignments: `a OP= b` lowers as `a = a OP b` on i64 operands.
			if (lhs.type_id != ctx.i64_type_id)
				throw std::runtime_error("'" + op.str() + "' currently only supports i64 operands");

			if (op == base::StrID("+=")) {
				value_to_store = builder.CreateAdd(lhs.value, rhs.value, "addassigntmp");
			} else if (op == base::StrID("-=")) {
				value_to_store = builder.CreateSub(lhs.value, rhs.value, "subassigntmp");
			} else if (op == base::StrID("*=")) {
				value_to_store = builder.CreateMul(lhs.value, rhs.value, "mulassigntmp");
			} else if (op == base::StrID("/=")) {
				value_to_store = builder.CreateSDiv(lhs.value, rhs.value, "divassigntmp");
			} else if (op == base::StrID("%=")) {
				value_to_store = builder.CreateSRem(lhs.value, rhs.value, "remassigntmp");
			} else {
				throw std::runtime_error(
					"Assignment operator '" + op.str()
					+ "' not yet supported (=, +=, -=, *=, /=, %= only)"
				);
			}
		}

		builder.CreateStore(value_to_store, lhs.address);

		result = LoweredExpr{
			.value   = value_to_store,
			.type_id = lhs.type_id,
		};
	}

	void ExprLowering::visitChainExpr(pst::Access<pst::expr::ChainExpr> chain) {
		const pst::ExprElement& atom_expr = *chain->getAtom().illegalAccess().value();
		LoweredExpr             current   = lower(atom_expr);

		for (pst::AccessLocked<pst::ExprElement> element_locked: chain->getChain()) {
			pst::Access<pst::ExprElement> element = element_locked.illegalAccess().value();

			// Call link: `callee(args)` — callee is whatever the chain resolved so far.
			if (auto opt_call = element.dynamicCast<pst::expr::Call>(); opt_call.has_value()) {
				current = emitCall(current, opt_call.value());
				continue;
			}

			auto opt_access = element.dynamicCast<pst::expr::Access>();
			if (!opt_access.has_value())
				throw std::runtime_error("Chain expression: only `.` access and calls are supported"
				);
			auto access = opt_access.value();

			// Namespace member access uses `.` just like field access, so dispatch on what
			// the chain has resolved to so far rather than on the operator.
			if (current.symbol.has_value()
			    && std::holds_alternative<sym::NamespaceSymbol>(
					ctx.getSymbol(current.symbol.value()).kind
				)) {
				const ns::NamespaceID ns_id
					= std::get<sym::NamespaceSymbol>(ctx.getSymbol(current.symbol.value()).kind)
				          .namespace_id;
				const base::StrID member_name = access->getName().illegalAccess().value()->unwrap();
				current = symbolRefToExpr(ctx.lookupInNamespace(ns_id, member_name), member_name);
				continue;
			}

			if (access->getType().illegalAccess().value()->unwrap() != base::StrID(".")) {
				throw std::runtime_error(
					"Chain expression: only `.` access supported, got: "
					+ access->getType().illegalAccess().value()->unwrap().str()
				);
			}

			if (current.address == nullptr)
				throw std::runtime_error("Field access on non-l-value expression");

			// Base must be a struct.
			tp::TypeInfo& base_type_info = ctx.getTypeInfo(current.type_id);
			if (!std::holds_alternative<tp::Structure>(base_type_info.kind)) {
				throw std::runtime_error(
					"Field access on non-struct type '" + base_type_info.name.str() + "'"
				);
			}

			// Look up the field in the struct's own namespace.
			const base::StrID field_name = access->getName().illegalAccess().value()->unwrap();
			sym::SymbolID     field_sym_id
				= ctx.lookupInScopeAndParents(base_type_info.namespace_id, field_name);
			const sym::Symbol& field_sym = ctx.getSymbol(field_sym_id);
			if (!std::holds_alternative<sym::FieldSymbol>(field_sym.kind)) {
				throw std::runtime_error(
					"Symbol '" + field_name.str() + "' in struct '" + base_type_info.name.str()
					+ "' is not a field"
				);
			}
			const u32 field_index = std::get<sym::FieldSymbol>(field_sym.kind).index;

			// GEP into the struct, then load the field's value. Array-typed fields
			// stay address-only, consumed by a following `[...]` link.
			llvm::Type*  struct_llvm_ty = lowering.lowerType(current.type_id, ctx);
			llvm::Value* field_ptr      = builder.CreateStructGEP(
                struct_llvm_ty, current.address, field_index, field_name.strView()
            );
			if (std::holds_alternative<tp::Array>(ctx.getTypeInfo(field_sym.type_id).kind)) {
				current = LoweredExpr{ .type_id = field_sym.type_id, .address = field_ptr };
				continue;
			}
			llvm::Type*  field_llvm_ty = lowering.lowerType(field_sym.type_id, ctx);
			llvm::Value* field_value
				= builder.CreateLoad(field_llvm_ty, field_ptr, field_name.strView());

			current = LoweredExpr{
				.value   = field_value,
				.type_id = field_sym.type_id,
				.address = field_ptr,  // result is still addressable, chains further
			};
		}

		result = current;
	}
}
