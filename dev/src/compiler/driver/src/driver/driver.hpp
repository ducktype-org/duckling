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

#include <expected>

namespace compiler::driver {
	enum class BackendType : std::uint8_t { LLVM, DVM };

	/**
	 * @brief The last intermediate representation of the module before the backends.
	 * It will be fed to the backends to generate the final output.
	 */
	struct BackendModuleData final {
		base::StrID                      module_id;
		std::vector<CRef<lir::Function>> functions;
	};

	/**
	 * @brief Compilation options.
	 */
	struct Options {
		BackendType backend_type;
		bool        compile_to_assembly;
		bool        dump_llvm_ir;

		/**
		 * Do not saves the compiled DBC to file.
		 * Useful when wanting to run the compiled bytecode.
		 */
		bool                     dvm_code_only_memory;
		bool                     add_builtin_library;
		std::vector<base::StrID> external_objects_files;
		std::vector<base::StrID> external_libs;
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
		CRef<Options> options;

	public:
		BackendDriver(CRef<Options> options): options(options) {}

		/**
		 * @brief Compile given the LIR functions and module data to the backend module.
		 * Outputs the module value.
		 * @param module_data
		 */
		virtual void compileModule(query::Context& ctx, const BackendModuleData& module_data) = 0;

		/**
		 * @brief Link all compiled modules into a single program.
		 */
		virtual void link(base::StrID output_file) = 0;

		/**
		 * @brief Run the compiled program. (only for DVM)
		 */
		virtual auto run() -> std::expected<RunOutput, std::string> = 0;

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

		/**
		 * @brief Compiles the HOUTUnit to the backend module.
		 */
		void compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit, base::StrID module_id);

		/**
		 * @brief Links all module compiled so far into a complete program.
		 */
		void link(base::StrID output_file);

		/**
		 * @brief Execute modules compiled with `compileModule` method.
		 * Only relevant for DVM backend.
		 * Returns the exit code of the executed program or an error message.
		 */
		auto run() -> std::expected<RunOutput, std::string>;

	private:
		Options            options;
		Box<BackendDriver> backend_driver;
	};
}
