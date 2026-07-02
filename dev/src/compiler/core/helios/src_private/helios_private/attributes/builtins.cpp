#include <helios/attributes/builtins.hpp>

#include <frontend/pst_parser/elements/hierarchy/expressions/string_value.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>

#include <diagnostic_interactive/placeholder.hpp>

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/query_errors.hpp>

namespace compiler::helios {
	namespace {
		/**
		 * @brief Single source of truth mapping builtin names to BuiltinType.
		 */
		const base::HashMap<std::string_view, BuiltinType>& builtinNameMapping() {
			static const base::HashMap<std::string_view, BuiltinType> mapping = {
				{ "ptr_from_slice", BuiltinType::RawPtrFromSlice },
			};
			return mapping;
		}
	}

	base::Optional<BuiltinType> builtinTypeFromStr(base::StrID name) {
		return builtinNameMapping().atMaybeCopy(name.strView());
	}

	base::StrID builtinTypeToStr(BuiltinType type) {
		switch (type) {
		case BuiltinType::RawPtrFromSlice:
			return base::StrID("ptr_from_slice");
		}
		CORE_UNREACHABLE();
	}

	BuiltinType parseBuiltinAttr(
		query::Context& ctx, base::Optional<pst::AccessLocked<pst::AtrArgList>> args
	) {
		if_opt_none(args) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				"Attribute 'builtin' expects exactly one argument, got 0.",
				base::Optional<dia_int::StablePosition>{}
			));
			query::throwFailed();
		}

		auto                                                    arg_list = args.value().unlock(ctx);
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

		auto builtin = builtinTypeFromStr(str_lit.value()->getValue().value);
		if_opt_none(builtin) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				base::strConcat("Unknown builtin '", str_lit.value()->getValue().value.str(), "'."),
				holder->getStablePosition()
			));
			query::throwFailed();
		}

		return builtin.value();
	}
}
