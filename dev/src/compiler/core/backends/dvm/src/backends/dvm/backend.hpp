
#include <lir/lir_structure/lir_structure.hpp>

#include <base/pointers/ref.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::backend_vm {

	struct BackendDVMGlobal {
		lir::LIRGlobal                      lir_global;
		base::Optional<CRef<lir::Function>> global_ctor;
		base::Optional<CRef<lir::Function>> global_dtor;
	};

	namespace internal {
		class ProgramLoweringContext;
	}

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
		// A Boxed pointer to allow forward declaration in order to hide implementation details.
		Box<internal::ProgramLoweringContext> program_context;
	};
}
