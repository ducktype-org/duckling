#include "type.hpp"

#include "frontend/pst_parser/elements/hierarchy/not_statements/code_block.hpp"

using namespace pg;
using namespace pg::tp;

namespace {
	class StructParser final: public pst::PstVisitorPanicky {
	public:
		StructParser(base::StrID name, TypeID id):
			type_info(type_info),
			id(id)
		{}

		TypeInformation type_info;

        void visitField(pst::Access<pst::Field> stmt) override {
            type_info.id_to_info[id].type.emplace<Structure>().fields.emplace_back(
                stmt->getName(),
                stmt->getTypeName().strView()
            );
        }
	};
}

pg::TypeInformation::TypeInformation(pg::LlvmProgramModule& llvm_module):
	  llvm_module(llvm_module) {}

void pg::TypeInformation::visitNamespace(pst::Access<pst::Namespace> stmt) {
	namespace_stack.push_back(stmt->getName());
	for (pst::AccessLocked<pst::LangElement> child: *stmt->getBody().illegalAccess().value())
		child.illegalAccess().value()->acceptVisitor(*this);
	namespace_stack.pop_back();
}

void pg::TypeInformation::visitClass(pst::Access<pst::Class> stmt) {

}
