#include "attributes.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/attribute_arg_list.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/query_errors.hpp>

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
				ctx.logInt(
					makeBox<dia_int::PlaceholderError>(
						base::strConcat(
							"Attribute '", attrNameStr(attr).str(), "' does not take arguments."
						),
						args.value().unlock(ctx)->getStablePosition()
					)
				);
				query::throwFailed();
			}
			return attr;
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
		}
		CORE_UNREACHABLE();
	}

	bool isValidForStmt(Attribute attr, pst::StmtKind kind) {
		variant_match(attr) {
			variant_case_novalue(NativeOnlyImpl, DVMOnlyImpl) { return kind == pst::StmtKind::Fun; }
			variant_case_novalue(BackendDependent) { return kind == pst::StmtKind::FunDecl; }
			variant_case_novalue(Builtin) { return kind == pst::StmtKind::FunDecl; }
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
			return std::unexpected(
				base::strConcat(
					"The attributes `",
					attrNameStr(NativeOnlyImpl{}),
					"' and '",
					attrNameStr(DVMOnlyImpl{}),
					"' are exclusive."
				)
			);
		}
		return {};
	}
}
