

#include "lir_unit.hpp"
#include <lir/lir_lowering/lir_lowering.hpp>
#include <base/extend_cpp/variant_match.hpp>

namespace compiler::lir {
    LIRUnit lowerToLIRUnit(query::Context& ctx, const mir::MIRUnit& mir_unit) {
        LIRUnit lir_unit;

        // Functions:
        for (const auto& mir_function: mir_unit.mir_functions) {
            CRef lir_function = ctx.query<lir::LowerToLIRFunction>({ mir_function });
            lir_unit.lir_functions.push_back(lir_function);
        }

        // Globals:
        for (const auto& mir_global: mir_unit.mir_globals) {
            CORE_ASSERT(mir_global.global.type.getType().carriesInformation(ctx), "Information-less global should have been discarded in MIR lowering");

            auto lir_global = LIRGlobal::fromMIR(ctx, mir_global.global);

            variant_match(mir_global.initial_value) {
                variant_case(ctv::CompileTimeValue, ctv_initial_value) {
                    lir_unit.lir_globals.emplace_back(LIRGlobalData{
                        .global = lir_global,
                        .data_initialization = ctv_initial_value,
                    });
                }
                variant_case(CRef<mir::Function>, mir_ctor_function) {
                    auto lir_ctor_function = ctx.query<lir::LowerToLIRFunction>({ mir_ctor_function });
                    lir_unit.lir_globals.emplace_back(LIRGlobalData{
                        .global = lir_global,
                        .data_initialization = LIRGlobalData::CTorDtorPair{
                            // @TODO: #929 add legit dtors when implemented 
                            .global_ctor = lir_ctor_function,
                            .global_dtor = std::nullopt,
                        },
                    });
                }
                variant_default {
                    CORE_PANIC("Unhandled MIRGlobalData initial value type in lowerToLIRUnit");
                }
            }
        }



        return lir_unit;
    }
}

