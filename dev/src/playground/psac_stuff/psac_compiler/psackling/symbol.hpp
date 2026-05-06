#include "namespace.hpp"
#include "type.hpp"

#include <base/extend_cpp/strongly_typed_id.hpp>

#include <string_id/string_id.hpp>

namespace pg::sym {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(SymbolID);

	struct Symbol {
		base::StrID     name;
		SymbolID        id;
		tp::TypeID      type_id;
		ns::NamespaceID namespace_id;
	};

}

ID_STD_HASH(pg::sym::SymbolID);

// 	class TypeInformation: public pst::PstVisitorPanicky {
// 	public:
// 		TypeInformation(pg::LlvmProgramModule& llvm_module);

// 		pg::LlvmProgramModule& llvm_module;

// 		void visitNamespace(pst::Access<pst::Namespace> stmt) override;
// 		void visitClass(pst::Access<pst::Class> stmt) override;


// 	private:
//         std::unordered_map<base::StrID, tp::TypeID> name_to_id;
// 		std::unordered_map<tp::TypeID, tp::TypeInfo> id_to_info;
// 		std::unordered_map<tp::TypeID, llvm::Type*>  id_to_llvm;

//         std::vector<base::StrID> namespace_stack = { base::StrID("") };
//         std::vector<base::StrID> ;
// 	};
// }
