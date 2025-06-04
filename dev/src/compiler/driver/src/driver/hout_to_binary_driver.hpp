/**
 * @author Wojciech Rzepliński
 * @brief Main module driver, currently compiles HOUT-Unit to LLVM Module.
 */
#pragma once

#include "backend_driver/backend_driver.hpp"

#include <artifacts/artifacts.hpp>
#include <helios/hout/hout.hpp>
#include <lir/lir_structure/lir_structure.hpp>

#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>

#include <expected>

namespace compiler::driver {

	/**
	 * @brief Main compilation driver of the HOUTUnit to backend module.
	 */
	class HoutToBinaryDriver final {
	public:
		HoutToBinaryDriver(BackendOptions opts):
			  options(std::move(opts)),
			  backend_driver(createBackendDriver(&options)) {}

		/**
		 * @brief Compiles the HOUTUnit to the backend module.
		 */
		void compileHOUTUnit(
			query::Context&              ctx,
			base::CRef<helios::HOUTUnit> hout_unit,
			base::StrID                  module_id,
			artifacts::FileArtifact      output_artifact
		);

		/**
		 * @brief Execute modules compiled with `compileModule` method.
		 * Only relevant for DVM backend.
		 * Returns the exit code of the executed program or an error message.
		 */
		auto run() -> std::expected<RunOutput, std::string>;

	private:
		BackendOptions     options;
		Box<BackendDriver> backend_driver;
	};
}
