#pragma once


#include <lir/lir_structure/lir_structure.hpp>

#include <string_id/string_id.hpp>

#include <ostream>

namespace compiler::driver {

	/**
	 * LIRGlobal with its optional constructor and destructor functions.
	 #2246 this should go
	 */
	struct LIRModuleGlobal final {
		lir::LIRGlobal lir_global;
		base::Optional<CRef<lir::Function>>
			global_ctor;  ///< Optional, if the global has a constructor.
		base::Optional<CRef<lir::Function>>
			global_dtor;  ///< Optional, if the global has a destructor.

		void debugPrint(query::Context& ctx, std::ostream& os) const;
	};

	/**
	 * @brief The last intermediate representation of the module before the backends.
	 * It will be fed to the backend operations to generate the final output.
	 */
	struct LIRModuleData final {
		/**
		 * @brief The module ID is more or a lass a module name, that will be use by the backend.
		 */
		base::StrID                      module_id;
		std::vector<CRef<lir::Function>> functions;
		std::vector<LIRModuleGlobal>
			globals;  ///< Global variables and their constructors/destructors.

		void debugPrint(query::Context& ctx, std::ostream& os) const;
	};
}
