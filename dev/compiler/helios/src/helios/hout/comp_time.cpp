#include "comp_time.hpp"
#include "elements/expr.hpp"

#include <query_framework/query_impl.hpp>

namespace compiler::helios {

    struct IMPLEMENT_QUERY(EvalExprToType, TypeEvalResult) {
        auto provide(Context& ctx, QKey key) -> PResult {
            // todo: query type from expr
            // get type from virtual functions
        }

        QUERY_AUTO_CACHE_COPY
    };
}
