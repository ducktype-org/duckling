#pragma once


#include <lir/lir_structure/lir_structure.hpp>

#include <string_id/string_id.hpp>

#include <ostream>

namespace compiler::driver {


	/**
	 * @brief The last intermediate representation of the module before the backends.
	 * It will be fed to the backend operations to generate the final output.
	 */
	struct LIRUnitWithBackendName final {
		/**
		 * @brief The module ID is more or a lass a module name, that will be use by the backend.
		 */
		base::StrID                      module_id;
		lir::LIRUnit 				     lir_unit;

		void debugPrint(query::Context& ctx, std::ostream& os) const;
	};
}
