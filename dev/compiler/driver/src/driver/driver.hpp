#pragma once

#include "backends/llvm/llvm_backend.hpp"
#include "base/ref.hpp"
#include "lir/lir_lowering/lir_lowering.hpp"
#include "lir/lir_structure/lir_structure.hpp"
#include "mir/mir_lowering/mir_lowering.hpp"
#include "query_framework/query_entry_point.hpp"
#include <base/ints.hpp>
#include <helios/hout/hout.hpp>

namespace compiler::driver {
	enum class BackendType { LLVM, DuckBC };

	class Driver final {
	public:
		Driver(BackendType backend_type);

		void compileHOUTUnit(base::CRef<helios::HOUTUnit> hout_unit) {
            switch (backend_type) {
                case BackendType::LLVM:
                    compileLLVM(hout_unit);
                    return;
                case BackendType::DuckBC:
                    compileDuckBC(hout_unit);
                    return;
            }
        }

	private:
		BackendType backend_type;
        
        auto getLIRFunctions(base::CRef<helios::HOUTUnit> hout_unit) -> std::vector<CRef<lir::Function>> {
            std::vector<CRef<lir::Function>> functions;
            functions.reserve(hout_unit->functions.size());

            for (const auto& hout_function: hout_unit->functions) {
                auto mir_function = query::entryPoint<mir::LowerToMirFunction>({ hout_function });
                auto lir_function = query::entryPoint<lir::LowerToLirFunction>({ mir_function });
                functions.push_back(lir_function);
            }

            return functions;
        }

        void compileLLVM(base::CRef<helios::HOUTUnit> hout_unit) {
            backend_llvm::Module mod;
        }

        void compileDuckBC(base::CRef<helios::HOUTUnit>) {
            // Not implemented
        }
	};
}
