#include <base/ref.hpp>
#include <lir/lir_structure/function_forward.hpp>

namespace compiler::backend_vm {
	class Module {
	public:
		Module() = default;

		[[nodiscard]] std::string serialize() const;

		static Module fromLirFunction(CRef<lir::Function> lir_function);

	private:
		// std::vector<
	};
}
