#include "attributes.hpp"

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/call_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/attribute.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/dotted_name.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/stmt_specifier.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/wrapper_elements/identifier_wrapper.hpp>

#include "base/except/exceptions.hpp"
#include <base/collections/maps.hpp>

namespace compiler::helios {
	using namespace attributes;
	base::Optional<Attribute> attrFromStr(base::StrID str) {
		static const auto mapping = [&] {
			return base::HashMap<std::string_view, Attribute>{
				{ "dvm_only_impl", Attribute::DVMOnlyImpl },
				{ "native_only_impl", attributes::NativeOnlyImpl },
				{ "backend_dependent", attributes::BackendDependent },
			};
		}();
		return mapping.atMaybeCopy(str.strView());
	}

#define DEFINE_EMPTY_ATTR_TO_STR(Attr, string) \
	template<>                                  \
	base::StrID attrToStr(const Attr&) {   \
		return base::StrID(string);             \
	}
	DEFINE_EMPTY_ATTR_TO_STR(DVMOnlyImpl, "dvm_only_impl")
	DEFINE_EMPTY_ATTR_TO_STR(NativeOnlyImpl,"native_only_impl")
	DEFINE_EMPTY_ATTR_TO_STR(BackendDependent, "backend_dependent")

#define DEFINE_IS_VALID_FOR_STMT(Attr, valid_kinds)\
	...

	bool isValidForStmt(Attribute attr, pst::StmtKind kind) {
		static const auto valid_stmts = [&] {
			return base::HashMap<Attribute, std::vector<pst::StmtKind>>{
				{ attributes::NativeOnlyImpl, { pst::StmtKind::Fun } },
				{ Attribute::DVMOnlyImpl, { pst::StmtKind::Fun } },
				{ attributes::BackendDependent, { pst::StmtKind::FunDecl } },
			};
		}();
		return std::ranges::any_of(valid_stmts.at(attr), [&](auto elem) { return elem == kind; });
	}

#define DEFINE_DISABLES_LOOKUP(Attr, disables) \
	...

	bool disablesLookup(Attribute attr) {
		switch (attr) {
		case Attribute::DVMOnlyImpl:
		case attributes::NativeOnlyImpl:
			return true;
		default:
			return false;
		}
	}

	namespace {
		bool hasAttribute(const std::vector<Attribute>& attributes, Attribute target) {
			return std::ranges::any_of(attributes, [target](const Attribute& a) {
				return a == target;
			});
		}
	}

	std::expected<std::monostate, std::string> validateAttributes(
		const std::vector<Attribute>& attributes
	) {
		if (hasAttribute(attributes, Attribute::DVMOnlyImpl)
		    and hasAttribute(attributes, attributes::NativeOnlyImpl)) {
			return std::unexpected(base::strConcat(
				"Tha attributes `",
				attrToStr(attributes::NativeOnlyImpl),
				"' and '",
				attrToStr(Attribute::DVMOnlyImpl),
				"' are exclusive."
			));
		}
		return {};
	}
}
