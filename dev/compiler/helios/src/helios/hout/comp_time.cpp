#include "comp_time.hpp"
#include "elements/expr.hpp"

#include <query_framework/query_impl.hpp>

#include "elements/query_hout_of_expr.hpp"
#include "visitors.hpp"

namespace compiler::helios {

    struct IMPLEMENT_QUERY(EvalExprToType, TypeEvalResult) {

        using ShortPathResult = errors::HResult<tsh::TypeInfo, CouldNotEvalShortPath, errors::Failed>;

        /**
         * A visitor to extract types from simple expression fast (i.e. short path it).
         * @note It might be changed to virtual function on expr in the future for performance.
         * For now it is kept as a visitor for code simplicity.
         */
        struct ShortPathVisitor: code::HoutExprVisitorPanicky {
            base::Optional<ShortPathResult> result;
            void output(ShortPathResult res) {
                CORE_ASSERT(result.empty(), "ShortPathVisitor already has a result");
                result.emplace(res);
            }



        };



        static auto provide(Context& ctx, QKey key) -> PResult {
            // todo: query type from expr
            // get type from virtual functions
            auto parsed = ctx.query<QueryHoutOfExpr>({ key.expr });
            if (parsed.hasError()) {
                return errors::HError(parsed.error());
            }
            
            ShortPathVisitor visitor;
            parsed.value()->acceptVisitor(visitor);
            auto short_path_result = visitor.result.value();

            if (short_path_result.hasError()) {
                variant_match(short_path_result.error()) {
                    variant_case(errors::Failed, _) {
                        return errors::HError(errors::Failed());
                    }
                    variant_case(CouldNotEvalShortPath, _) {
                        throw base::NotYetImplemented("Comp time when short-path eval failed");
                    }
                    variant_default {
                        CORE_PANIC("Unhandler error in EvalExprToType");
                    }
                }
            }

            return short_path_result.value();
        }

        QUERY_AUTO_CACHE_COPY
    };

    QUERY_IMPLEMENTATION_BOILERPLATE(EvalExprToType);
}
