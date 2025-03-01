#include <base/ref.hpp>
#include <lir/lir_structure/function_forward.hpp>
#include "backends/vm/elements.hpp"
#include "builders.hpp"

namespace compiler::backend_vm {
	class Module {
	public:
		Module() = default;

		void addLirFunction(CRef<lir::Function> lir_function);

		[[nodiscard]] std::string serialize() const;

	private:
		std::vector<CodeFile> files{};
	};
}
