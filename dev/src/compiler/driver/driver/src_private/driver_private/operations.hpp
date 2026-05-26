#pragma once

#include "lir_module_data.hpp"

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
	 #2246 this should go!
	 */
	DECLARE_QUERY(
		CompileHOUTUnitToLIRModuleData,
		CompileHOUTUnitToLIRModuleDataKey,
		CRef<query::QResult<LIRUnitWithBackendName>>,
		({})
	)

	/**
	 * @brief Query that produces LIRUnitWithBackendName for given Duckling module.
	 */
	DECLARE_QUERY(
		CompileToLIRModuleData, frontend::ModuleID, CRef<query::QResult<LIRUnitWithBackendName>>, ({})
	)
}
