#pragma once

#include "base/box.hpp"
#include "frontend/module_tree/module_tree.hpp"
#include <backends/llvm/llvm_backend.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>
#include <lir/lir_lowering/lir_lowering.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_lowering.hpp>
#include <query_framework/query_entry_point.hpp>
#include <base/ints.hpp>
#include <helios/hout/hout.hpp>

namespace compiler::driver {
	enum class BackendType { LLVM, DuckBC };

	struct LIRModule {
		frontend::ModuleID               module_id;
		std::vector<CRef<lir::Function>> functions;
	};

	struct Options {
		BackendType backend_type;
		base::StrID output_file;
	};

	class BackendStrategy {
		CRef<Options> options;

	public:
		BackendStrategy(CRef<Options> options): options(options) {}

		virtual void compile(LIRModule& lir_module) = 0;

		virtual ~BackendStrategy() = default;
	};

	class LLVMBackendStrategy: public BackendStrategy {
	public:
		LLVMBackendStrategy(CRef<Options> options): BackendStrategy(options) {}

		void compile(LIRModule& lir_module) override;
	};

	class DuckBCBackendStrategy: public BackendStrategy {
	public:
		DuckBCBackendStrategy(CRef<Options> options): BackendStrategy(options) {}

		void compile(LIRModule&) override {
			// Not implemented
		}
	};

	Box<BackendStrategy> createBackendStrategy(CRef<Options> options);

	class Driver final {
	public:
		Driver(Options options):
			  options(options),
			  backend_driver(createBackendStrategy(&options)) {}

		void compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit, frontend::ModuleID module_id);

	private:
		Options              options;
		Box<BackendStrategy> backend_driver;
	};
}
