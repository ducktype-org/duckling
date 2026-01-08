#pragma once

#include "lir_module_data.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout_fd.hpp>

#include <functional>

namespace compiler::driver {

	struct compileHOUTUnitToLIRModuleDataKey {
		CRef<helios::HOUTUnit> hout_unit;
		base::StrID            module_name;

		auto operator<=>(const compileHOUTUnitToLIRModuleDataKey&) const = default;

		[[nodiscard]] u64 queryUnstablePerfectHash() const {
			return std::hash<base::StrID>{}(module_name);
		}
	};

	/**
	 * @brief Query that converts HOUTUnit to LIRModuleData.
	 */
	DECLARE_QUERY(
		compileHOUTUnitToLIRModuleData,
		compileHOUTUnitToLIRModuleDataKey,
		query::QResult<LIRModuleData>,
		({})
	)

	/**
	 * @brief Query that produces LIRModuleData for given Duckling module.
	 */
	DECLARE_QUERY(CompileToLIRModuleData, frontend::ModuleID, query::QResult<LIRModuleData>, ({}))
}
