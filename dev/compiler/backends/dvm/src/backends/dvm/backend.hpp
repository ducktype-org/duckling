
#include <lir/lir_structure/function_forward.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <base/ref.hpp>

#include <vm/bytecode/builders/builders.hpp>
#include <vm/bytecode/bytecode.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief Represents a module which maps to a single VM file.
	 */
	class Module {
		base::StrID module_id;

	public:
		Module(base::StrID module_id, const std::vector<CRef<lir::Function>>& functions);

		/**
		 * @brief Builds a module representation.
		 */
		[[nodiscard]] vm::code::CodeCollection build() const;

	private:
		vm::code::builders::TypeContextBuilder type_context_builder;
		vm::code::CodeCollection               code;
	};
}
