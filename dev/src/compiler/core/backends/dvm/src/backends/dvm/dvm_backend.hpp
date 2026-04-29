
#include "dvm_internal_fwd.hpp"

#include <debug_info/debug_info.hpp>
#include <lir/lir_structure/lir_structure.hpp>

#include <base/pointers/ref.hpp>

#include <query_framework/context/context_fd.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief A statefull collection of code lowered into VM bytecode.
	 * @note If used improperly, query_ctx might become a dangling reference.
	 */
	class DVMCodeBuilder final {
	public:
		/**
		 * @brief Construct a new DVMCodeBuilder object
		 *
		 * @param query_ctx
		 * @param build_debug_info Whether to build debug info for the module.
		 * @param is_comp_time_lowering Whether we are lowering the code to be loaded by the VM for
		 * compile time evaluation, or for the final output module.
		 */
		DVMCodeBuilder(query::Context& query_ctx, bool build_debug_info, bool is_comp_time_lowering);

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
		 * @brief Insert raw bytecode into a module.
		 */
		void insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode);

		/**
		 * @brief Produces the per-module bytecode collection without cross-module validation.
		 * Full validation is performed at link time after all modules are merged.
		 */
		[[nodiscard]] vm::code::CodeCollection build() const;

		/**
		 * @brief Builds the debug info for the module if the class
		 * was constructed with build_debug_info=true. Returns empty optional otherwise.
		 * @note Only the first call returns the value and the subsequent calls will
		 * return an empty optional.
		 */
		[[nodiscard]] base::Optional<debug_info::DebugInfo> buildDebugInfo();

	private:
		// A Boxed pointer to allow forward declaration in order to hide implementation details.
		Box<internal::ProgramLoweringContext> program_context;

		bool build_debug_info;
	};
}
