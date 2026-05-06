#include "llvm.hpp"


namespace pg::sym {
	class GlobalSymbols {
		public:
			GlobalSymbols() = default;

			void addSymbol(base::StrID name, tp::TypeID type_id) {
				symbols.emplace(name, type_id);
			}
		private:
			std::unordered_map<base::StrID, tp::TypeID> symbols;
	};

	class TypeInformation: public pst::PstVisitorPanicky {
	public:
		TypeInformation(pg::LlvmProgramModule& llvm_module);

		pg::LlvmProgramModule& llvm_module;

		void visitNamespace(pst::Access<pst::Namespace> stmt) override;
		void visitClass(pst::Access<pst::Class> stmt) override;


	private:
        std::unordered_map<base::StrID, tp::TypeID> name_to_id;
		std::unordered_map<tp::TypeID, tp::TypeInfo> id_to_info;
		std::unordered_map<tp::TypeID, llvm::Type*>  id_to_llvm;

        std::vector<base::StrID> namespace_stack = { base::StrID("") };
        std::vector<base::StrID> ;
	};
}
