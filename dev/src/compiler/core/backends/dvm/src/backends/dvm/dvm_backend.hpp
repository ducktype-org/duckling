
#include "dvm_internal_fwd.hpp"

#include <lir/lir_structure/lir_structure.hpp>

#include <base/pointers/ref.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::backend_vm {

	/**
	 * @brief A statefull collection of code lowered into VM bytecode.
	 */
	class Module {
		base::StrID module_id;

	public:
		Module(base::StrID module_id);

		/**
		 * @brief Inserts a LIR function into the module.
		 */
		void insertLirFunction(CRef<lir::Function> lir_function);

		/**
		 * @brief Inserts an extern C function into the module.
		 */
		void insertExternCFunction(const vm::code::ExternalCFunction& extern_func);

		/**
		 * @brief Inserts a LIR global into the module.
		 */
		void insertLirGlobal(
			const lir::LIRGlobal&               lir_global,
			base::Optional<CRef<lir::Function>> global_ctor,
			base::Optional<CRef<lir::Function>> global_dtor
		);

		/**
		 * @brief Insert raw bytecode into a module. Currently used by compile time evaluations to
		 * insert functions needed for CTE.
		 */
		void insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode);

		/**
		 * @brief Validates and builds module's representation as DVM program.
		 */
		[[nodiscard]] vm::code::CodeCollection build() const;

	private:
		// A Boxed pointer to allow forward declaration in order to hide implementation details.
		Box<internal::ProgramLoweringContext> program_context;
	};
}
