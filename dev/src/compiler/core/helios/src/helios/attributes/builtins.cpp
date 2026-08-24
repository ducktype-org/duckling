#include <frontend/pst_parser/elements/hierarchy/expressions/string_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/hout_creation/definition_generation/default_destructors.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/query_errors.hpp>

namespace compiler::helios {
	namespace {
		/**
		 * @brief Single source of truth mapping builtin names to BuiltinKind.
		 */
		const base::HashMap<std::string_view, BuiltinKind>& builtinNameMapping() {
			static const base::HashMap<std::string_view, BuiltinKind> mapping = {
				{ "ptr_from_slice", BuiltinKind::PtrFromSlice },
				{ "slice_from_ptr_len", BuiltinKind::SliceFromPtrLen },
				{ "dvm_alloc_arr", BuiltinKind::DvmAllocArr },
				{ "dvm_realloc_arr", BuiltinKind::DvmReallocArr },
				{ "dvm_free_arr", BuiltinKind::DvmFreeArr },
				{ "dvm_alloc", BuiltinKind::DvmAlloc },
				{ "dvm_free", BuiltinKind::DvmFree },
				{ "size_of", BuiltinKind::SizeOf },
				{ "alignment_of", BuiltinKind::AlignmentOf },
				{ "move_out", BuiltinKind::MoveOut },
				{ "move_in", BuiltinKind::MoveIn },
				{ "dvm_ptr_parts", BuiltinKind::DvmPtrParts },
				{ "dvm_is_nullptr", BuiltinKind::DvmIsNullptr },
				{ "dvm_nullptr", BuiltinKind::DvmNullptr },
			};
			return mapping;
		}
	}

	base::StrID builtinKindToStr(BuiltinKind type) {
		switch (type) {
		case BuiltinKind::PtrFromSlice:
			return base::StrID("ptr_from_slice");
		case BuiltinKind::SliceFromPtrLen:
			return base::StrID("slice_from_ptr_len");
		case BuiltinKind::DvmAllocArr:
			return base::StrID("dvm_alloc_arr");
		case BuiltinKind::DvmReallocArr:
			return base::StrID("dvm_realloc_arr");
		case BuiltinKind::DvmFreeArr:
			return base::StrID("dvm_free_arr");
		case BuiltinKind::DvmAlloc:
			return base::StrID("dvm_alloc");
		case BuiltinKind::DvmFree:
			return base::StrID("dvm_free");
		case BuiltinKind::SizeOf:
			return base::StrID("size_of");
		case BuiltinKind::AlignmentOf:
			return base::StrID("alignment_of");
		case BuiltinKind::MoveOut:
			return base::StrID("move_out");
		case BuiltinKind::MoveIn:
			return base::StrID("move_in");
		case BuiltinKind::DvmPtrParts:
			return base::StrID("dvm_ptr_parts");
		case BuiltinKind::DvmIsNullptr:
			return base::StrID("dvm_is_nullptr");
		case BuiltinKind::DvmNullptr:
			return base::StrID("dvm_nullptr");
		case BuiltinKind::BoxAlloc:
			return base::StrID("box_alloc");
		case BuiltinKind::BoxFree:
			return base::StrID("box_free");
		case BuiltinKind::BoxDestructor:
			return base::StrID("box_destructor");
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
			ctx.logInt(makeBox<dia::PlaceholderError>(
				"Attribute 'builtin' expects exactly one argument, got 0.",
				base::Optional<dia::StablePosition>{}
			));
			query::throwFailed();
		}

		auto arg_list = args.value().unlock(ctx);
		std::vector<pst::AccessLocked<pst::UniversalExprHolder>> holders{ arg_list->begin(),
			                                                              arg_list->end() };

		if (holders.size() != 1) {
			ctx.logInt(makeBox<dia::PlaceholderError>(
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
			ctx.logInt(makeBox<dia::PlaceholderError>(
				"Attribute 'builtin' expects a string literal naming the builtin.",
				holder->getStablePosition()
			));
			query::throwFailed();
		}

		auto builtin = builtinKindFromStr(str_lit.value()->getValue().value);
		if_opt_none(builtin) {
			ctx.logInt(makeBox<dia::PlaceholderError>(
				base::strConcat("Unknown builtin '", str_lit.value()->getValue().value.str(), "'."),
				holder->getStablePosition()
			));
			query::throwFailed();
		}

		return builtin.value();
	}

	HOUTFunction getBuiltinImpl(query::Context& ctx, SymID symbol, BuiltinKind type) {
		using namespace code::shorthands;
		const Shorthand s{ ctx };

		switch (type) {
		case BuiltinKind::PtrFromSlice: {
			auto& decl         = ctx.query<QueryDeclOfFun>(symbol)->valueOrThrow();
			auto  slice_type   = decl.parameters.at(0).type.getType().as<tsh::SliceAbstractType>();
			auto  slice_fields = ctx.query<QuerySliceTypeData>(slice_type);

			auto body
				= StmtPack{ s.ret(s.access(
								s.ident(decl.parameters.at(0).helios_symbol), slice_fields->ptr
							)) }
			          .toCodeBlock();
			return {
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(std::move(body)),
			};
		}
		case BuiltinKind::SliceFromPtrLen: {
			auto&      decl            = ctx.query<QueryDeclOfFun>(symbol)->valueOrThrow();
			const auto result_sym_type = decl.return_type;
			auto       slice_type      = result_sym_type.getType().as<tsh::SliceAbstractType>();
			auto       slice_fields    = ctx.query<QuerySliceTypeData>(slice_type);

			using Variable = defgen::GeneratedFunctionVariable;

			// var __result: slice T = <default>;
			const SymID result_symbol = ctx.query<defgen::QueryGeneratedSymbol>({
				.name                  = base::StrID("__result"),
				.generated_symbol_data = Variable{ .function_symbol = symbol,
			                                       .variable_index  = 0,
			                                       .type            = result_sym_type },
			});

			auto body = StmtPack{
				// var __result: slice T = <default>;
				s.var(
					result_symbol, result_sym_type, s.defaultValue(result_sym_type.getType())
				),
				// __result.ptr = p;
				s.assign(
					s.access(s.ident(result_symbol), slice_fields->ptr),
					s.ident(decl.parameters.at(0).helios_symbol)
				),
				// __result.len = l;
				s.assign(
					s.access(s.ident(result_symbol), slice_fields->len),
					s.ident(decl.parameters.at(1).helios_symbol)
				),
				// return __result;
				s.ret(s.ident(result_symbol)),
			}.toCodeBlock();

			return HOUTFunction(
				code::generatedOrigin(),
				&decl,
				std::make_shared<const code::CodeBlock>(std::move(body))
			);
		}
		case BuiltinKind::SizeOf:
		case BuiltinKind::AlignmentOf: {
			// `size_of(v: meta) -> i64` / `alignment_of(v: meta) -> i64` simply return the
			// corresponding unary meta operator applied to the parameter. The operator is lowered
			// through MIR/LIR to a `Meta` instruction and evaluated at compile time.
			auto&      decl = ctx.query<QueryDeclOfFun>(symbol)->valueOrThrow();
			const auto op   = (type == BuiltinKind::SizeOf) ? code::BuiltinUnary::SizeOf
			                                                : code::BuiltinUnary::AlignOf;

			std::vector<Box<code::Stmt>> body{};
			body.emplace_back(makeBox<code::ReturnStmt>(
				code::generatedOrigin(),
				makeBox<code::UnaryOperatorExpr>(
					ctx,
					code::generatedOrigin(),
					op,
					makeBox<code::IdentifierExpr>(
						ctx, code::generatedOrigin(), decl.parameters.at(0).helios_symbol
					)
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
		case BuiltinKind::BoxDestructor: {
			// `box_destructor(b: box T)` destroys the pointee, then frees the box storage. The
			// pointee type `T` is the pointee of the `box T` parameter.
			auto&      decl         = ctx.query<QueryDeclOfFun>(symbol)->valueOrThrow();
			const auto pointee_type = decl.parameters.at(0).type.getType();
			return defgen::buildBoxDestructor(ctx, pointee_type);
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
		case BuiltinKind::PtrFromSlice:
			return BuiltinOrigin::HOUT;
		case BuiltinKind::SliceFromPtrLen:
			return BuiltinOrigin::HOUT;
		case BuiltinKind::DvmAllocArr:
		case BuiltinKind::DvmReallocArr:
		case BuiltinKind::DvmFreeArr:
		case BuiltinKind::DvmAlloc:
		case BuiltinKind::DvmFree:
		case BuiltinKind::DvmPtrParts:
		case BuiltinKind::DvmIsNullptr:
		case BuiltinKind::DvmNullptr:
			return BuiltinOrigin::DVMBackend;
		case BuiltinKind::SizeOf:
		case BuiltinKind::AlignmentOf:
			return BuiltinOrigin::HOUT;
		case BuiltinKind::MoveOut:
		case BuiltinKind::MoveIn:
			return BuiltinOrigin::LIR;
		case BuiltinKind::BoxAlloc:
		case BuiltinKind::BoxFree:
			return BuiltinOrigin::DVMBackend | BuiltinOrigin::NativeBackend;
		case BuiltinKind::BoxDestructor:
			return BuiltinOrigin::HOUT;
		}

		CORE_UNREACHABLE();
	}

	SymID boxAllocSymForType(query::Context& ctx, tsh::AbstractType pointee_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = builtinKindToStr(BuiltinKind::BoxAlloc),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ tsh::SymbolType<>::withDefaults(pointee_type),
		                                      defgen::BuiltinTemplatedSymbol::Kind::BoxAlloc },
		});
	}

	SymID boxFreeSymForType(query::Context& ctx, tsh::AbstractType pointee_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = builtinKindToStr(BuiltinKind::BoxFree),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ tsh::SymbolType<>::withDefaults(pointee_type),
		                                      defgen::BuiltinTemplatedSymbol::Kind::BoxFree },
		});
	}

	SymID boxDestructorSymForType(query::Context& ctx, tsh::AbstractType pointee_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = builtinKindToStr(BuiltinKind::BoxDestructor),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ tsh::SymbolType<>::withDefaults(pointee_type),
		                                      defgen::BuiltinTemplatedSymbol::Kind::BoxDestructor },
		});
	}

	SymID moveInSymForType(query::Context& ctx, tsh::SymbolType<> element_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = builtinKindToStr(BuiltinKind::MoveIn),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ element_type,
		                                      defgen::BuiltinTemplatedSymbol::Kind::MoveIn },
		});
	}

	Box<code::Expr> makeBoxAllocCall(
		query::Context& ctx, code::ElementOrigin origin, Box<code::Expr> inner
	) {
		using namespace code::shorthands;
		const Shorthand s{ ctx };
		const auto      pointee_type = inner->expression_type.getSymbolType().getType();

		// This helper preserves the caller-supplied `origin` rather than the builders' default
		// `generatedOrigin()`, so both the callee identifier and the call itself carry it.
		auto callee = withOrigin(origin, s.ident(boxAllocSymForType(ctx, pointee_type)));
		return withOrigin(origin, s.call(std::move(callee), std::move(inner)));
	}

}
