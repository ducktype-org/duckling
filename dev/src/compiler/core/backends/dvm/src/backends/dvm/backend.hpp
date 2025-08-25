
#include <lir/lir_structure/function_forward.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <base/ref.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/validator/type_validator.hpp>
#include <vm/bytecode/validator/valid_program.hpp>

namespace compiler::backend_vm {

	struct BackendDVMGlobal {
		lir::LirGlobal                      lir_global;
		base::Optional<CRef<lir::Function>> global_ctor;
		base::Optional<CRef<lir::Function>> global_dtor;
	};

	/**
	 * @brief A statefull collection of code lowered into VM bytecode.
	 * @note Currently it does not support dynamic function insertion, but it will.
	 */
	class Module {
		base::StrID module_id;

	public:
		Module(
			query::Context&                         ctx,
			base::StrID                             module_id,
			const std::vector<CRef<lir::Function>>& functions,
			const std::vector<BackendDVMGlobal>&    globals
		);

		/**
		 * @brief Builds a module representation.
		 */
		[[nodiscard]] vm::code::CodeCollection build() const;

	private:
		vm::code::ValidProgram valid_program = vm::code::ValidProgram::withStdlib();
	};
}
