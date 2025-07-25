#pragma once


#include <lir/lir_structure/lir_structure.hpp>

#include <base/string_id.hpp>

namespace compiler::driver {
	struct LIRModuleGlobal final {
		lir::LirGlobal lir_global;
		base::Optional<CRef<lir::Function>>
			global_ctor;  ///< Optional, if the global has a constructor.
		base::Optional<CRef<lir::Function>>
			global_dtor;  ///< Optional, if the global has a destructor.
	};

	/**
	 * @brief The last intermediate representation of the module before the backends.
	 * It will be fed to the backend operations to generate the final output.
	 */
	struct LIRModuleData final {
		base::StrID                      module_id;
		std::vector<CRef<lir::Function>> functions;
		std::vector<LIRModuleGlobal>
			globals;  ///< Global variables and their constructors/destructors.
	};
}
