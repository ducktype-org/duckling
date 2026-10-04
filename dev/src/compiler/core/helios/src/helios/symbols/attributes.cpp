#include "attributes.hpp"

#include <frontend/pst_parser/elements/hierarchy/expressions/string_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/attribute_arg_list.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios_private/hout_creation/expressions/hout_of_subexpr.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/query_errors.hpp>

#include <algorithm>
#include <functional>

namespace compiler::helios {
	using namespace attributes;

	namespace {
		using AttrArgs = base::Optional<pst::AccessLocked<pst::AtrArgList>>;

		/**
		 * @brief A parser that turns an argument list into a concrete Attribute.
		 * On invalid arguments it logs a diagnostic and fails the current query.
		 */
		using AttrParser = std::function<Attribute(query::Context&, AttrArgs)>;

		/**
		 * @brief Helper for attributes that take no arguments.
		 */
		Attribute expectNoArgs(query::Context& ctx, Attribute attr, AttrArgs args) {
			if (args.has_value() && args.value().unlock(ctx)->size() > 0) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					base::strConcat(
						"Attribute '", attrNameStr(attr).str(), "' does not take arguments."
					),
					args.value().unlock(ctx)->getStablePosition()
				));
				query::throwFailed();
			}
			return attr;
		}

		/**
		 * @brief Parses the single unsigned-integer argument of `@cffi_variadic_fixed_params(<n>)`.
		 * On invalid arguments it logs a diagnostic and fails the current query.
		 */
		u64 parseU64(query::Context& ctx, AttrArgs args, std::string_view error_msg) {
			if_opt_none(args) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					std::string(error_msg), base::Optional<dia::StablePosition>{}
				));
				query::throwFailed();
			}

			auto arg_list = args.value().unlock(ctx);
			std::vector<pst::AccessLocked<pst::UniversalExprHolder>> holders{ arg_list->begin(),
				                                                              arg_list->end() };

			if (holders.size() != 1) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					std::string(error_msg), arg_list->getStablePosition()
				));
				query::throwFailed();
			}

			auto holder      = holders.front().unlock(ctx);
			auto elem        = code::subExprFromPST(ctx, holder->getExpr()).valueOrThrow();
			auto numeric_opt = dynamic_cast<code::LiteralNumericExpr*>(elem.get());

			base::Optional<i64> value{};
			if (numeric_opt != nullptr) value = numeric_opt->value.coerceTo<i64>();

			if (not value.has_value() or value.value() < 0) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					std::string(error_msg), arg_list->getStablePosition()
				));
				query::throwFailed();
			}
			return base::safeIntConv<u64>(value.value());
		}

		/**
		 * @brief Whether `name` is a valid C identifier: `[A-Za-z_][A-Za-z0-9_]*`.
		 */
		bool isCIdentifier(std::string_view name) {
			auto is_alpha = [](char c) {
				return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or c == '_';
			};
			auto is_alnum = [&](char c) { return is_alpha(c) or (c >= '0' and c <= '9'); };

			if (name.empty() or not is_alpha(name.front())) return false;
			return std::ranges::all_of(name, is_alnum);
		}

		/**
		 * @brief Parses the single string-literal argument of `@c_symbol_name("<name>")`.
		 * On invalid arguments it logs a diagnostic and fails the current query.
		 */
		base::StrID parseCSymbolName(query::Context& ctx, AttrArgs args) {
			constexpr std::string_view ERROR_MSG
				= "Attribute 'c_symbol_name' expects exactly one string literal argument.";

			if_opt_none(args) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					std::string(ERROR_MSG), base::Optional<dia::StablePosition>{}
				));
				query::throwFailed();
			}

			auto arg_list = args.value().unlock(ctx);
			std::vector<pst::AccessLocked<pst::UniversalExprHolder>> holders{ arg_list->begin(),
				                                                              arg_list->end() };

			if (holders.size() != 1) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					std::string(ERROR_MSG), arg_list->getStablePosition()
				));
				query::throwFailed();
			}

			auto holder  = holders.front().unlock(ctx);
			auto str_lit = holder->getExpr().unlock(ctx).dynamicCast<pst::expr::ExprStrValue>();
			if_opt_none(str_lit) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					std::string(ERROR_MSG), holder->getStablePosition()
				));
				query::throwFailed();
			}

			// The name is written to the object file and to the `.dbc` as-is, so it must be a C
			// identifier. Only that syntax is checked: a C name that is a `.dbc` keyword (e.g.
			// `variant`) still links with LLVM but is rejected by the DVM backend.
			auto name = str_lit.value()->getValue().value;
			if (not isCIdentifier(name.strView())) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					base::strConcat(
						"Attribute 'c_symbol_name' expects a valid C identifier, got '",
						name.str(),
						"'."
					),
					holder->getStablePosition()
				));
				query::throwFailed();
			}
			return name;
		}
	}

	base::Optional<Attribute> attrFromStr(query::Context& ctx, base::StrID name, AttrArgs args) {
		static const base::HashMap<std::string_view, AttrParser> mapping = {
			{ "dvm_only_impl",
			  [](query::Context& c, AttrArgs a) { return expectNoArgs(c, DVMOnlyImpl{}, a); } },
			{ "native_only_impl",
			  [](query::Context& c, AttrArgs a) { return expectNoArgs(c, NativeOnlyImpl{}, a); } },
			{ "backend_dependent",
			  [](query::Context& c, AttrArgs a) { return expectNoArgs(c, BackendDependent{}, a); } },
			{ "builtin",
			  [](query::Context& c, AttrArgs a) -> Attribute {
				  return Builtin{ .builtin = parseBuiltinAttr(c, a) };
			  } },
			{ "cffi_variadic_fixed_params",
			  [](query::Context& c, AttrArgs a) -> Attribute {
				  return CFFIVariadicFunction{
					  .fixed_params
					  = parseU64(c, a, "Attribute requires one, non-negative integer argument.")
				  };
			  } },
			{ "c_symbol_name",
			  [](query::Context& c, AttrArgs a) -> Attribute {
				  return CSymbolName{ .name = parseCSymbolName(c, a) };
			  } },
		};

		auto parser = mapping.atMaybeCopy(name.strView());
		if_opt_none(parser) return {};
		return parser.value()(ctx, args);
	}

	base::StrID attrNameStr(Attribute attr) {
		variant_match(attr) {
			variant_case_novalue(DVMOnlyImpl) { return base::StrID("dvm_only_impl"); }
			variant_case_novalue(NativeOnlyImpl) { return base::StrID("native_only_impl"); }
			variant_case_novalue(BackendDependent) { return base::StrID("backend_dependent"); }
			variant_case_novalue(Builtin) { return base::StrID("builtin"); }
			variant_case_novalue(CFFIVariadicFunction) {
				return base::StrID("cffi_variadic_fixed_params");
			}
			variant_case_novalue(CSymbolName) { return base::StrID("c_symbol_name"); }
		}
		CORE_UNREACHABLE();
	}

	bool isValidForStmt(Attribute attr, pst::StmtKind kind) {
		variant_match(attr) {
			variant_case_novalue(NativeOnlyImpl, DVMOnlyImpl) { return kind == pst::StmtKind::Fun; }
			variant_case_novalue(BackendDependent) { return kind == pst::StmtKind::FunDecl; }
			variant_case_novalue(Builtin) { return kind == pst::StmtKind::FunDecl; }
			variant_case_novalue(CFFIVariadicFunction) { return kind == pst::StmtKind::FunDecl; }
			variant_case_novalue(CSymbolName) { return kind == pst::StmtKind::FunDecl; }
		}
		CORE_UNREACHABLE();
	}

	bool disablesLookup(Attribute attr) { return v_matches(attr, DVMOnlyImpl, NativeOnlyImpl); }

	template<typename Attr>
	base::Optional<CRef<Attr>> getAttrInVector(const std::vector<Attribute>& attrs) {
		for (auto& attr: attrs) if_opt_some(base::maybeChoose<Attr>(attr), value) return value;
		return {};
	}

#define MAKE_ATTR_INSTANCE(attr) \
	template base::Optional<CRef<attr>> getAttrInVector<attr>(const std::vector<Attribute>& attrs);
	FOR_EACH(MAKE_ATTR_INSTANCE, ATTRIBUTES_LIST)

	template<typename Attr>
	bool hasAttrInVector(const std::vector<Attribute>& attrs) {
		return getAttrInVector<Attr>(attrs).has_value();
	}

	std::expected<std::monostate, std::string> validateAttributes(
		const std::vector<Attribute>& attributes
	) {
		if (hasAttrInVector<DVMOnlyImpl>(attributes)
		    and hasAttrInVector<NativeOnlyImpl>(attributes)) {
			return std::unexpected(base::strConcat(
				"The attributes `",
				attrNameStr(NativeOnlyImpl{}),
				"' and '",
				attrNameStr(DVMOnlyImpl{}),
				"' are exclusive."
			));
		}
		return {};
	}
}
