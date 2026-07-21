#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/string_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/query_errors.hpp>

namespace compiler::helios {
	namespace {
		/**
		 * @brief Single source of truth mapping builtin names to BuiltinKind.
		 */
		const base::HashMap<std::string_view, BuiltinKind>& builtinNameMapping() {
			static const base::HashMap<std::string_view, BuiltinKind> mapping = {
				{ "char_ptr_from_slice", BuiltinKind::CharPtrFromSlice },
				{ "char_slice_from_ptr_len", BuiltinKind::CharSliceFromPtrLen },
				{ "dvm_char_alloc", BuiltinKind::DvmCharAlloc },
				{ "dvm_char_realloc", BuiltinKind::DvmCharRealloc },
				{ "dvm_char_free", BuiltinKind::DvmCharFree },
			};
			return mapping;
		}
	}

	base::StrID builtinKindToStr(BuiltinKind type) {
		switch (type) {
		case BuiltinKind::CharPtrFromSlice:
			return base::StrID("char_ptr_from_slice");
		case BuiltinKind::CharSliceFromPtrLen:
			return base::StrID("char_slice_from_ptr_len");
		case BuiltinKind::DvmCharAlloc:
			return base::StrID("dvm_char_alloc");
		case BuiltinKind::DvmCharRealloc:
			return base::StrID("dvm_char_realloc");
		case BuiltinKind::DvmCharFree:
			return base::StrID("dvm_char_free");
		case BuiltinKind::BoxAlloc:
			return base::StrID("box_alloc");
		case BuiltinKind::BoxFree:
			return base::StrID("box_free");
		case BuiltinKind::ListFree:
			return base::StrID("list_free");
		}
		CORE_UNREACHABLE();
	}

	/**
	 * @brief Map a builtin name to its BuiltinKind, empty when the name is unknown.
	 */
	base::Optional<BuiltinKind> builtinKindFromStr(base::StrID name) {
		return builtinNameMapping().atMaybeCopy(name.strView());
	}

	BuiltinKind parseBuiltinAttr(
		query::Context& ctx, base::Optional<pst::AccessLocked<pst::AtrArgList>> args
	) {
		if_opt_none(args) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				"Attribute 'builtin' expects exactly one argument, got 0.",
				base::Optional<dia_int::StablePosition>{}
			));
			query::throwFailed();
		}

		auto arg_list = args.value().unlock(ctx);
		std::vector<pst::AccessLocked<pst::UniversalExprHolder>> holders{ arg_list->begin(),
			                                                              arg_list->end() };

		if (holders.size() != 1) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				base::strConcat(
					"Attribute 'builtin' expects exactly one argument, got ", holders.size(), "."
				),
				arg_list->getStablePosition()
			));
			query::throwFailed();
		}

		auto holder  = holders.front().unlock(ctx);
		auto str_lit = holder->getExpr().unlock(ctx).dynamicCast<pst::expr::ExprStrValue>();
		if_opt_none(str_lit) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				"Attribute 'builtin' expects a string literal naming the builtin.",
				holder->getStablePosition()
			));
			query::throwFailed();
		}

		auto builtin = builtinKindFromStr(str_lit.value()->getValue().value);
		if_opt_none(builtin) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				base::strConcat("Unknown builtin '", str_lit.value()->getValue().value.str(), "'."),
				holder->getStablePosition()
			));
			query::throwFailed();
		}

		return builtin.value();
	}

	HOUTFunction getBuiltinImpl(query::Context& ctx, SymID symbol, BuiltinKind type) {
		switch (type) {
		case BuiltinKind::CharPtrFromSlice: {
			// `char_ptr_from_slice(slice T s) -> manyptr T` simply returns the slice's data pointer
			// field. This mirrors the `length` method, only reading a different field (`ptr` vs
			// `len`).
			auto& decl         = ctx.query<QueryDeclOfFun>(symbol)->valueOrThrow();
			auto  slice_type   = decl.parameters.at(0).type.getType().as<tsh::SliceAbstractType>();
			auto  slice_fields = ctx.query<QuerySliceTypeData>(slice_type);

			std::vector<Box<code::Stmt>> body{};
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::AccessExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(
						ctx, code::generatedOrigin(), decl.parameters.at(0).helios_symbol
					),
					slice_fields->ptr
				)
			));
			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}
		case BuiltinKind::CharSliceFromPtrLen: {
			// `char_slice_from_ptr_len(p: ptr char, l: u64) -> slice char` builds a slice value
			// out of a data pointer and a length. This mirrors the implicit class constructor:
			// declare a default-initialized result, assign each field from a parameter, then
			// return it.
			auto&      decl            = ctx.query<QueryDeclOfFun>(symbol)->valueOrThrow();
			const auto result_sym_type = decl.return_type;
			auto       slice_type      = result_sym_type.getType().as<tsh::SliceAbstractType>();
			auto       slice_fields    = ctx.query<QuerySliceTypeData>(slice_type);

			using Variable = defgen::GeneratedFunctionVariable;

			// var __result: slice char = <default>;
			const SymID result_symbol = ctx.query<defgen::QueryGeneratedSymbol>({
				.name                  = base::StrID("__result"),
				.generated_symbol_data = Variable{ .function_symbol = symbol,
			                                       .variable_index  = 0,
			                                       .type            = result_sym_type },
			});

			std::vector<Box<code::Stmt>> body{};
			// - One declaration, one assignment per field, one return.
			body.reserve(4);
			body.emplace_back(makeBox<code::VariableStmt>(
				code::generatedOrigin(),
				makeBox<code::DefaultValueExpr>(
					ctx, code::generatedOrigin(), result_sym_type.getType()
				),
				result_sym_type,
				result_symbol
			));

			// __result.ptr = p;
			body.emplace_back(makeBox<code::AssignmentStmt>(
				code::generatedOrigin(),
				makeBox<code::AccessExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_symbol),
					slice_fields->ptr
				),
				makeBox<code::IdentifierExpr>(
					ctx, code::generatedOrigin(), decl.parameters.at(0).helios_symbol
				)
			));

			// __result.len = l;
			body.emplace_back(makeBox<code::AssignmentStmt>(
				code::generatedOrigin(),
				makeBox<code::AccessExpr>(
					ctx,
					code::generatedOrigin(),
					makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_symbol),
					slice_fields->len
				),
				makeBox<code::IdentifierExpr>(
					ctx, code::generatedOrigin(), decl.parameters.at(1).helios_symbol
				)
			));

			// return __result;
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::IdentifierExpr>(ctx, code::generatedOrigin(), result_symbol)
			));

			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(code::CodeBlock{
					.statements = std::move(body),
				})
			);
		}
		default: {
			CORE_PANIC(
				base::strConcat("Builtin `", builtinKindToStr(type), "` is not implemented in HOUT")
			);
		}
		}
		CORE_UNREACHABLE();
	}

	BuiltinOrigins getBuiltinOrigins(BuiltinKind type) {
		switch (type) {
		case BuiltinKind::CharPtrFromSlice:
			return BuiltinOrigin::HOUT;
		case BuiltinKind::CharSliceFromPtrLen:
			return BuiltinOrigin::HOUT;
		case BuiltinKind::DvmCharAlloc:
			return BuiltinOrigin::DVMBackend;
		case BuiltinKind::DvmCharRealloc:
			return BuiltinOrigin::DVMBackend;
		case BuiltinKind::DvmCharFree:
			return BuiltinOrigin::DVMBackend;
		case BuiltinKind::BoxAlloc:
		case BuiltinKind::BoxFree:
		case BuiltinKind::ListFree:
			return BuiltinOrigin::DVMBackend | BuiltinOrigin::NativeBackend;
		}

		CORE_UNREACHABLE();
	}

	SymID boxAllocSymForType(query::Context& ctx, tsh::AbstractType pointee_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = base::StrID("box_alloc"),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ .type = pointee_type,
		                                      .kind
		                                      = defgen::BuiltinTemplatedSymbol::Kind::BoxAlloc },
		});
	}

	SymID boxFreeSymForType(query::Context& ctx, tsh::AbstractType pointee_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = base::StrID("box_free"),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ .type = pointee_type,
		                                      .kind
		                                      = defgen::BuiltinTemplatedSymbol::Kind::BoxFree },
		});
	}

	SymID listFreeSymForType(query::Context& ctx, tsh::AbstractType element_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = base::StrID("list_free"),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ .type = element_type,
		                                      .kind
		                                      = defgen::BuiltinTemplatedSymbol::Kind::ListFree },
		});
	}

	Box<code::Expr> makeBoxAllocCall(
		query::Context& ctx, code::ElementOrigin origin, Box<code::Expr> inner
	) {
		const auto pointee_type = inner->expression_type.getSymbolType().getType();

		std::vector<Box<code::Expr>> args;
		args.emplace_back(std::move(inner));
		return makeBox<code::CallExpr>(
			ctx,
			origin,
			makeBox<code::IdentifierExpr>(ctx, origin, boxAllocSymForType(ctx, pointee_type)),
			std::move(args)
		);
	}
}
