#pragma once

#include "backend_options.hpp"

#include <artifacts/artifacts.hpp>
#include <lir/lir_structure/lir_structure.hpp>

#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>

#include <expected>

namespace compiler::driver {
	/**
	 * @brief The last intermediate representation of the module before the backends.
	 * It will be fed to the backends to generate the final output.
	 */
	struct BackendModuleData final {
		base::StrID                      module_id;
		std::vector<CRef<lir::Function>> functions;
	};

	struct RunOutput final {
		int exit_code;
	};

	/**
	 * @brief Backend strategy interface.
	 * Backends will have unique logic for compiling the module, so they only need to implement
	 * `compile` method.
	 * @todo for future, see if we can just make "Module" interface with 'add_function'-like methods
	 */
	class BackendDriver {
	protected:
		CRef<BackendOptions> options;

	public:
		BackendDriver(CRef<BackendOptions> options): options(options) {}

		/**
		 * @brief Compile given the LIR functions and module data to the backend module.
		 * Outputs the module value.
		 * @param module_data
		 */
		virtual void compileModule(
			query::Context&          ctx,
			const BackendModuleData& module_data,
			artifacts::FileArtifact  output_artifact
		) = 0;

		/**
		 * @brief Run the compiled program. (only for DVM)
		 */
		virtual auto run() -> std::expected<RunOutput, std::string> = 0;

		virtual ~BackendDriver() = default;
	};

	Box<BackendDriver> createBackendDriver(CRef<BackendOptions> options);
}
