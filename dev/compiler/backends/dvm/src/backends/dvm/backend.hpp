#include <base/ref.hpp>
#include <lir/lir_structure/function_forward.hpp>
#include <typesystem/lower/type_layout.hpp>
#include "builders.hpp"

namespace compiler::backend_vm {
	/**
	 * @brief Represents a module which maps to a single VM file.
	 */
	class Module {
		base::StrID module_id;

	public:
		Module(base::StrID module_id);

		/**
		* @brief Inserts a function into the module.
		*/
		void addLirFunction(CRef<lir::Function> lir_function);

		/**
		* @brief Builds a module representation as parse-able bytecode.
		*/
		void buildRepr(std::ostream& out) const;

	private:
		CodeFileBuilder file_builder;
	};
}
