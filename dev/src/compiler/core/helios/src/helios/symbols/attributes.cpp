#include "attributes.hpp"

#include <frontend/pst_parser/elements/includes/basic.hpp>

#include <base/collections/maps.hpp>
#include <base/extend_cpp/variant_match.hpp>

namespace compiler::helios {
	using namespace attributes;

	base::Optional<Attribute> attrFromStr(base::StrID str) {
		static const auto mapping = [&] {
			return base::HashMap<std::string_view, Attribute>{
				{ "dvm_only_impl", DVMOnlyImpl{} },
				{ "native_only_impl", NativeOnlyImpl{} },
				{ "backend_dependent", BackendDependent{} },
			};
		}();
		return mapping.atMaybeCopy(str.strView());
	}

	base::StrID attrNameStr(Attribute attr) {
		variant_match(attr) {
			variant_case_novalue(DVMOnlyImpl) { return base::StrID("dvm_only_impl"); }
			variant_case_novalue(NativeOnlyImpl) { return base::StrID("native_only_impl"); }
			variant_case_novalue(BackendDependent) { return base::StrID("backend_dependent"); }
		}
		CORE_UNREACHABLE();
	}

	bool isValidForStmt(Attribute attr, pst::StmtKind kind) {
		variant_match(attr) {
			variant_case_novalue(NativeOnlyImpl, DVMOnlyImpl) { return kind == pst::StmtKind::Fun; }
			variant_case_novalue(BackendDependent) { return kind == pst::StmtKind::FunDecl; }
		}
		CORE_UNREACHABLE();
	}

	bool disablesLookup(Attribute attr) { return v_matches(attr, DVMOnlyImpl, NativeOnlyImpl); }

	namespace {
		template<typename Attr>
		bool hasAttr(const std::vector<Attribute>& attrs) {
			return std::ranges::any_of(attrs, [](const Attribute& a) {
				return base::holds<Attr>(a);
			});
		}
	}

	std::expected<std::monostate, std::string> validateAttributes(
		const std::vector<Attribute>& attributes
	) {
		if (hasAttr<DVMOnlyImpl>(attributes) and hasAttr<NativeOnlyImpl>(attributes)) {
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
