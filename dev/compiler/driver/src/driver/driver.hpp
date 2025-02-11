/**
 * @file driver.hpp
 * @author Wojciech Rzepliński
 * @brief Main module driver, currently compiles HOUT-Unit to LLVM Module.
 */
#pragma once

#include <base/box.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <backends/llvm/llvm_backend.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>
#include <cstdint>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/query_entry_point.hpp>
#include <base/ints.hpp>
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
		base::StrID output_file;
	};

	/**
	 * @brief Backend strategy interface.
	 * Backends will have unique logic for compiling the module, so they only need to implement
	 * `compile` method.
	 */
	class BackendDriver {
		CRef<Options> options;  // NOLINT: Currently unused

	public:
		BackendDriver(CRef<Options> options): options(options) {}

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

	Box<BackendDriver> createBackendStrategy(CRef<Options> options);

	/**
	 * @brief Main compilation driver of the HOUTUnit to backend module.
	 */
	class Driver final {
	public:
		Driver(Options options):
			  options(options),
			  backend_driver(createBackendStrategy(&options)) {}

		void compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit, base::StrID module_id);

	private:
		Options            options;
		Box<BackendDriver> backend_driver;
	};
}
