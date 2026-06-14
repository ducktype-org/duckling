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
	base::Optional<Attribute> attrFromStr(base::StrID str) {
		static const auto mapping = [&] {
			return base::HashMap<std::string_view, Attribute>{
				{ "dvm_only_impl", Attribute::DVMOnlyImpl },
				{ "native_only_impl", Attribute::NativeOnlyImpl },
				{ "backend_dependent", Attribute::BackendDependent },
			};
		}();
		return mapping.atMaybeCopy(str.strView());
	}

	base::StrID attrToStr(Attribute attr) {
		switch (attr) {
		case Attribute::DVMOnlyImpl:
			return base::StrID("dvm_only_impl");
		case Attribute::NativeOnlyImpl:
			return base::StrID("native_only_impl");
		case Attribute::BackendDependent:
			return base::StrID("backend_dependent");
		default:
			CORE_PANIC("Attribute serialization not implemented");
		}
	}

	bool isValidForStmt(Attribute attr, pst::StmtKind kind) {
		static const auto valid_stmts = [&] {
			return base::HashMap<Attribute, std::vector<pst::StmtKind>>{
				{ Attribute::NativeOnlyImpl, { pst::StmtKind::Fun } },
				{ Attribute::DVMOnlyImpl, { pst::StmtKind::Fun } },
				{ Attribute::BackendDependent, { pst::StmtKind::FunDecl } },
			};
		}();
		return std::ranges::any_of(valid_stmts.at(attr), [&](auto elem) { return elem == kind; });
	}

	bool disablesLookup(Attribute attr) {
		switch (attr) {
		case Attribute::DVMOnlyImpl:
		case Attribute::NativeOnlyImpl:
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
		    and hasAttribute(attributes, Attribute::NativeOnlyImpl)) {
			return std::unexpected(base::strConcat(
				"Tha attributes `",
				attrToStr(Attribute::NativeOnlyImpl),
				"' and '",
				attrToStr(Attribute::DVMOnlyImpl),
				"' are exclusive."
			));
		}
		return {};
	}
}
