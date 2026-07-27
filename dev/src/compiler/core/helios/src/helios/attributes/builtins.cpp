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
#include <helios_private/hout_creation/definition_generation/default_destructors.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
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
				{ "size_of", BuiltinKind::SizeOf },
				{ "alignment_of", BuiltinKind::AlignmentOf },
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
		case BuiltinKind::SizeOf:
			return base::StrID("size_of");
		case BuiltinKind::AlignmentOf:
			return base::StrID("alignment_of");
		case BuiltinKind::BoxAlloc:
			return base::StrID("box_alloc");
		case BuiltinKind::BoxFree:
			return base::StrID("box_free");
		case BuiltinKind::ListFree:
			return base::StrID("list_free");
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
		using namespace code::shorthands;
		const Shorthand s{ ctx };

		switch (type) {
		case BuiltinKind::CharPtrFromSlice: {
			// `char_ptr_from_slice(slice T s) -> manyptr T` simply returns the slice's data pointer
			// field. This mirrors the `length` method, only reading a different field (`ptr` vs `len`).
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

			auto body = StmtPack{
				// var __result: slice char = <default>;
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
		case BuiltinKind::SizeOf:
		case BuiltinKind::AlignmentOf:
			return BuiltinOrigin::HOUT;
		case BuiltinKind::BoxAlloc:
		case BuiltinKind::BoxFree:
		case BuiltinKind::ListFree:
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
			= defgen::BuiltinTemplatedSymbol{ pointee_type,
		                                      defgen::BuiltinTemplatedSymbol::Kind::BoxAlloc },
		});
	}

	SymID boxFreeSymForType(query::Context& ctx, tsh::AbstractType pointee_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = builtinKindToStr(BuiltinKind::BoxFree),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ pointee_type,
		                                      defgen::BuiltinTemplatedSymbol::Kind::BoxFree },
		});
	}

	SymID listFreeSymForType(query::Context& ctx, tsh::AbstractType element_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = builtinKindToStr(BuiltinKind::ListFree),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ element_type,
		                                      defgen::BuiltinTemplatedSymbol::Kind::ListFree },
		});
	}

	SymID boxDestructorSymForType(query::Context& ctx, tsh::AbstractType pointee_type) {
		return ctx.query<defgen::QueryGeneratedSymbol>({
			.name = builtinKindToStr(BuiltinKind::BoxDestructor),
			.generated_symbol_data
			= defgen::BuiltinTemplatedSymbol{ pointee_type,
		                                      defgen::BuiltinTemplatedSymbol::Kind::BoxDestructor },
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
