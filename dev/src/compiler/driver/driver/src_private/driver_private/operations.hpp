#pragma once

#include "lir_unit_with_name.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout_fd.hpp>

#include <functional>

namespace compiler::driver {

	struct CompileHOUTUnitToLIRModuleDataKey final {
		CRef<helios::HOUTUnit> hout_unit;
		base::StrID            module_name;

		auto operator<=>(const CompileHOUTUnitToLIRModuleDataKey&) const = default;

		[[nodiscard]] u64 queryUnstablePerfectHash() const {
			return std::hash<base::StrID>{}(module_name);
		}
	};

	/**
	 * @brief Query that converts HOUTUnit to LIRUnitWithBackendName.
	 * @TODO: #2246 Consider removing this query and moving logic from it elsewhere
	 * or changing it into function (it only call unit lowering and generates IR debug artifacts).
	 */
	DECLARE_QUERY(
		CompileHOUTUnitToLIRModuleData,
		CompileHOUTUnitToLIRModuleDataKey,
		CRef<query::QResult<LIRUnitWithBackendName>>,
		({})
	)

	/**
	 * @brief Query that produces LIRUnitWithBackendName for given Duckling module.
	 * @TODO: #2246 Consider removing this query and moving logic from it elsewhere
	 * or changing it into function (it only adds a module name).
	 */
	DECLARE_QUERY(
		CompileToLIRModuleData,
		frontend::ModuleID,
		CRef<query::QResult<LIRUnitWithBackendName>>,
		({})
	)
}
