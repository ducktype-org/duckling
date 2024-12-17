#include "comp_time.hpp"
#include "elements/expr.hpp"

#include <query_framework/query_impl.hpp>

#include "elements/query_hout_of_expr.hpp"

namespace compiler::helios {

    struct IMPLEMENT_QUERY(EvalExprToType, TypeEvalResult) {
        auto provide(Context& ctx, QKey key) -> PResult {
            // todo: query type from expr
            // get type from virtual functions
            auto parsed = ctx.query<QueryHoutOfExpr>({ key.expr });
            if (parsed.hasError()) {
                return errors::HError(parsed.error());
            }

            auto type = parsed.value()->evalToType(ctx);

            if (type.hasError()) {
                variant_match(type.error()) {
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

            return type.value();
        }

        QUERY_AUTO_CACHE_COPY
    };
}
