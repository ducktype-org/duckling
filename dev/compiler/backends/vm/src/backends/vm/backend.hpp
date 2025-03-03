#include <base/ref.hpp>
#include <lir/lir_structure/function_forward.hpp>
#include <typesystem/lower/type_layout.hpp>
#include "backends/vm/elements.hpp"
#include "builders.hpp"

namespace compiler::backend_vm {
	/**
	 * @brief Represents a module which maps to a single file.
	 */
	class Module {
		base::StrID module_id;

	public:
		Module(base::StrID module_id);

		void addLirFunction(CRef<lir::Function> lir_function);

		void buildRepr(std::ostream& out) const;

	private:
		CodeFileBuilder file_builder;
	};
}
