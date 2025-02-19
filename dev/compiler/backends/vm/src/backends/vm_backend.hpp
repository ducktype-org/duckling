#include <string>

namespace compiler::backend_vm {
	/**
	 * This structure represents a single vm file.
	 */
	class Module {
	public:
		Module() {}

		[[nodiscard]] std::string serialize() const;

	private:
		// std::vector<
	};
}
