#pragma once


#include <lir/lir_structure/lir_structure.hpp>

#include <string_id/string_id.hpp>

#include <ostream>

namespace compiler::driver {


	/**
	 * @brief A simple struct holding a LIRUnit together with a module name that will be used by the
	 * backends. It is effective the last intermediate representation thet will be used to with
	 * backend operations to generate the final output.
	 */
	struct LIRUnitWithBackendName final {
		/**
		 * @brief The module ID is more or a lass a module name, that will be use by the backend.
		 */
		base::StrID  module_id;
		lir::LIRUnit lir_unit;

		void debugPrint(query::Context& ctx, std::ostream& os) const;
	};
}
