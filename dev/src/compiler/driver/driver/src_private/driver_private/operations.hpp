#pragma once

#include "lir_module_data.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout_fd.hpp>

namespace compiler::driver {

	struct CompileHOUTUnitToLIRModuleDataKey final {
		CRef<helios::HOUTUnit> hout_unit;
		base::StrID            module_name;

		auto operator<=>(const CompileHOUTUnitToLIRModuleDataKey&) const = default;

		[[nodiscard]] base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Query that converts HOUTUnit to LIRModuleData.
	 */
	DECLARE_QUERY(
		CompileHOUTUnitToLIRModuleData,
		CompileHOUTUnitToLIRModuleDataKey,
		CRef<query::QResult<LIRModuleData>>,
		({})
	)

	/**
	 * @brief Query that produces LIRModuleData for given Duckling module.
	 */
	DECLARE_QUERY(
		CompileToLIRModuleData, frontend::ModuleID, CRef<query::QResult<LIRModuleData>>, ({})
	)
}
