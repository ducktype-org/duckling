#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/string_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/types.hpp>

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
				{ "ptr_from_slice", BuiltinKind::RawPtrFromSlice },
			};
			return mapping;
		}
	}

	base::Optional<BuiltinKind> builtinKindFromStr(base::StrID name) {
		return builtinNameMapping().atMaybeCopy(name.strView());
	}

	base::StrID builtinKindToStr(BuiltinKind type) {
		switch (type) {
		case BuiltinKind::RawPtrFromSlice:
			return base::StrID("ptr_from_slice");
		}
		CORE_UNREACHABLE();
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
		case BuiltinKind::RawPtrFromSlice: {
			// `ptr_from_slice(slice T s) -> manyptr T` simply returns the slice's data pointer
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
		default: {
			CORE_PANIC(
				base::strConcat("Builtin `", builtinKindToStr(type), "` is not implemented in HOUT")
			);
		}
		}
		CORE_UNREACHABLE();
	}

	BuiltinOrigins builtinOrigin(BuiltinKind type) {
		switch (type) {
		case BuiltinKind::RawPtrFromSlice:
			return BuiltinOrigin::HOUT;
		}
		CORE_UNREACHABLE();
	}
}
