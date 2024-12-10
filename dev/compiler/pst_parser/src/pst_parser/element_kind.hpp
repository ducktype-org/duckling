#pragma once

#include <base/stringifyable_enum.hpp>


namespace pst {
    /**
     * PST element kind.
     * @note Same groups of elements are under the same kind.
     * @note Not a full list, since we don't need it for all elements yet.
     * We can extend it when needed.
     * @todo: sort it?
     */
    enum class ElementKind {
        TopLevel,
        Import,

        CodeBlock,
        CodeBlockOrStmt,

        ClassBlock,
        
        // Duckling declarations:
        Namespace,
        Class,
        Variable,
        Fun,
        Block,
        
        Using,
        Alias,

        Const,


        // Duckling statements:
        If,
        While,
        For,

        // Actions:
        Action,

        // Expressions:
        ExprStmt,

        // for all expression elements:
        ExprElement,
        
        
        // for detecting when kind was not set:
        KindNotSet,
    };
}

