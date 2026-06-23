#pragma once

#include <helios/symbols/symbol_id.hpp>
#include <ctv/ctv.hpp>


#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::templates {

    // enum class TemplateKind {
    //     Function,
    //     Class,
    //     Namespace,
    //     Const,
    // };


    struct TemplateBakeKey final {
        SymID template_sym_id;

        // TODO: we will have to assert argument types!
        std::vector<ctv::CompileTimeValue> template_arguments;

        [[nodiscard]]
        base::Bit256 queryUnstablePerfectHash() const;
    };


    /**
     * @brief Data produced by baking a template symbol ID.
     * TODO: this will be passed to PST root in type-opaque way, so other helios code can retrieve it when needed. 
     */
    struct TemplateBakePSTLinkedData final {
        // TemplateKind kind;
        // SymID instantiated_sym_id;

        std::vector<SymID> template_arguments_symbols;

        // std::vector<ctv::CompileTimeValue> template_arguments;
    };

    /**
     * @brief Query to bake a template symbol ID.
     * @param key The template bake key.
     * @return The baked symbol ID.
     */
    DECLARE_QUERY(QueryBakeTemplateSymID, TemplateBakeKey, query::QResult<SymID>, ({}));


    // TODO: bake to hout units?

    
}