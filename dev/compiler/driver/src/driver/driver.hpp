/**
 * @file driver.hpp
 * @author Wojciech Rzepliński
 * @brief Main module driver, currently compiles HOUT-Unit to LLVM Module.
 */
#pragma once

#include <helios/hout/hout.hpp>
#include <lir/lir_structure/lir_structure.hpp>

#include <base/box.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>

namespace compiler::driver {
	enum class BackendType : std::uint8_t { LLVM, DVM };

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
		BackendType              backend_type;
		base::StrID              output_file;
		bool                     compile_to_assembly;
		bool                     dump_llvm_ir;
		bool                     add_builtin_library;
		std::vector<base::StrID> external_objects_files;
		std::vector<base::StrID> external_libs;
	};

	/**
	 * @brief Backend strategy interface.
	 * Backends will have unique logic for compiling the module, so they only need to implement
	 * `compile` method.
	 * @todo for future, see if we can just make "Module" interface with 'add_function'-like methods
	 */
	class BackendDriver {
	protected:
		CRef<Options> options;

	public:
		BackendDriver(CRef<Options> options): options(options) {}

		/**
		 * @brief Compile given the LIR functions and module data to the backend module.
		 * Outputs the module value.
		 * @param module_data
		 */
		virtual void compileModule(const BackendModuleData& module_data) = 0;

		virtual void link() = 0;

		virtual ~BackendDriver() = default;
	};

	Box<BackendDriver> createBackendDriver(CRef<Options> options);

	/**
	 * @brief Main compilation driver of the HOUTUnit to backend module.
	 */
	class Driver final {
	public:
		Driver(Options opts):
			  options(std::move(opts)),
			  backend_driver(createBackendDriver(&options)) {}

		void compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit, base::StrID module_id);

	private:
		Options            options;
		Box<BackendDriver> backend_driver;
	};
}
