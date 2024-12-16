#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <typesystem/higher/type_info.hpp>

#include <helios/helios_errors.hpp>
#include "../helios_result.hpp"


namespace compiler::helios {

    using TypeEvalResult = errors::HResult<tsh::TypeInfo, errors::Failed>;

    struct KeyOf_EvalExprToType {
        Ref<pst::ExprElement> expr;
        // todo: hash, ==
        // todo: make template for pst-query keys
    };
    
    /**
     * Given the PST element, evaluates type of that element
     */
    DECLARE_QUERY(EvalExprToType, KeyOf_EvalExprToType, TypeEvalResult)
}
