#pragma once

#include <helios/mangler/mangler.hpp>
#include <helios/queries/queries.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <lir/lir_lowering/lir_unit.hpp>
#include <lir/lir_structure/lir_structure.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <string_view>
#include <tuple>

namespace compiler::lir::test_utils {

	/**
	 * @brief Collection of functions compiled to LIR, and their HOUT and MIR counterparts.
	 */
	struct LIRModuleResult final {
		frontend::ModuleID module;
		helios::ScopeID    scope;

		lir::LIRUnit lir_unit;

		base::Map<base::StrID, std::tuple<CRef<helios::HOUTFunction>, CRef<lir::Function>>> funcs{};
		base::Map<base::StrID, std::tuple<CRef<helios::HOUTGlobalData>, lir::LIRGlobalData>>
			globals{};

		[[nodiscard]] CRef<helios::HOUTFunction> houtFunc(std::string_view name) const {
			return std::get<CRef<helios::HOUTFunction>>(funcs.at(base::StrID(name)));
		}

		[[nodiscard]] CRef<lir::Function> lirFunc(std::string_view name) const {
			return std::get<CRef<lir::Function>>(funcs.at(base::StrID(name)));
		}

		[[nodiscard]] CRef<helios::HOUTGlobalData> houtGlobal(std::string_view name) const {
			return std::get<CRef<helios::HOUTGlobalData>>(globals.at(base::StrID(name)));
		}

		[[nodiscard]] lir::LIRGlobalData lirGlobalData(std::string_view name) const {
			return std::get<lir::LIRGlobalData>(globals.at(base::StrID(name)));
		}
	};

	/**
	 * Compiles the module at given path to LIR, returning also HOUT counterparts of
	 * functions and globals for easier testing.
	 *
	 * Note that the functions don't include any global constructors/destructors that might be
	 * generated for global variables, but they are included in the LIRGlobalData for the global
	 * variables, so they can be accessed in tests if needed.
	 */
	inline LIRModuleResult getLIROfModule(std::string_view module_path) {
		auto [module, scope] = compiler::helios::test_utils::getModule(fs::File(module_path));

		base::Optional<LIRModuleResult> result;

		query::utils::withContextDo([&](query::Context& ctx) {
			helios::HOUTUnit unit = ctx.query<helios::QueryModuleHOUT>(module)->valueOrPanic();

			auto mir_unit = mir::lowerToMIRUnit(ctx, &unit);
			CORE_ASSERT(mir_unit.hasValue(), "MIR lowering failed!");

			auto lir_unit = lir::lowerToLIRUnit(ctx, mir_unit.valueOrPanic());

			result.emplace(LIRModuleResult{ .module = module, .scope = scope, .lir_unit = lir_unit }
			);

			// Now we additionally to the lowering also create a mapping from HOUT functions/globals
			// to their LIR counterparts, to easily navigate between those levels in tests. We do it
			// based on mangled names. This should be stable, but beware that if mangling logic or
			// usage changes this might break and require adjustments.

			for (const auto& hout_func: unit.functions) {
				auto mangled_name = helios::mangler::getSimpleMangledName(
					ctx, hout_func->declaration->original_symbol
				);

				// This is O(n^2), but it should be fine in unit tests with small modules.
				for (const auto& lir_func: lir_unit.lir_functions) {
					if (lir_func->mangled_name == mangled_name) {
						result->funcs.put(
							hout_func->declaration->original_name,
							std::make_tuple(hout_func, lir_func)
						);
						break;
					}
				}
				CORE_ASSERT(
					result->funcs.contains(base::StrID(hout_func->declaration->original_name)),
					base::strConcat(
						"Failed to find LIR function for HOUT function: ",
						hout_func->declaration->original_name.strView()
					)
				);
			}

			for (const auto& hout_glob: unit.glob_data) {
				auto mangled_name
					= helios::mangler::getSimpleMangledName(ctx, hout_glob->helios_symbol);

				// This is O(n^2), but it should be fine in unit tests with small modules.
				for (const auto& lir_global: lir_unit.lir_globals) {
					if (lir_global.global.mangled_name == mangled_name) {
						result->globals.put(
							hout_glob->original_name, std::make_tuple(hout_glob, lir_global)
						);
						break;
					}
				}
				CORE_ASSERT(
					result->globals.contains(base::StrID(hout_glob->original_name)),
					base::strConcat(
						"Failed to find LIR global for HOUT global: ",
						hout_glob->original_name.strView()
					)
				);
			}
		});
		return result.value();
	}
}
