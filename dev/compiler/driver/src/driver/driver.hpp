/**
 * @file driver.hpp
 * @author Wojciech Rzepliński
 * @brief Main module driver, currently compiles HOUT-Unit to LLVM Module.
 */
#pragma once

#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <helios/hout/hout.hpp>

namespace compiler::driver {
	enum class BackendType : std::uint8_t { LLVM, DuckBC };

	/**
	 * @brief The last intermediate representation of the module before the backends.
	 * It will be fed to the backends to generate the final output.
	 */
	struct BackendModuleData {
		base::StrID                      module_id;
		std::vector<CRef<lir::Function>> functions;
	};

	/**
	 * @brief Compilation options.
	 */
	struct Options {
		BackendType backend_type;
		base::StrID output_file;  /// just a mock-up for now
	};

	/**
	 * @brief Backend strategy interface.
	 * Backends will have unique logic for compiling the module, so they only need to implement
	 * `compile` method.
	 * @todo for future, see if we can just make "Module" interface with 'add_function'-like methods
	 */
	class BackendDriver {
		CRef<Options> options;  // NOLINT: Currently unused

	public:
		BackendDriver(CRef<Options> options): options(options) {}

		/**
		 * @brief Compile given the LIR functions and module data to the backend module.
		 * Outputs the module value.
		 * @param module_data
		 */
		virtual void compile(BackendModuleData& module_data) = 0;

		virtual ~BackendDriver() = default;
	};

	class LLVMBackendDriver: public BackendDriver {
	public:
		LLVMBackendDriver(CRef<Options> options): BackendDriver(options) {}

		void compile(BackendModuleData& module_data) override;
	};

	class DuckBCBackendDriver: public BackendDriver {
	public:
		DuckBCBackendDriver(CRef<Options> options): BackendDriver(options) {}

		void compile(BackendModuleData&) override {
			throw base::NotYetImplemented("compilation for BC driver");
		}
	};

	Box<BackendDriver> createBackendDriver(CRef<Options> options);

	/**
	 * @brief Main compilation driver of the HOUTUnit to backend module.
	 */
	class Driver final {
	public:
		Driver(Options options): options(options), backend_driver(createBackendDriver(&options)) {}

		void compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit, base::StrID module_id);

	private:
		Options            options;
		Box<BackendDriver> backend_driver;
	};
}
