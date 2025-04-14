/**
 * This file holds structure defining mir-lifetime-scopes,
 * and mir-lifetime-scope-tree. There are the structure
 * that then MIR uses to deduce lifetime scopes of variables.
 *
 * MIR scopes are in many ways similar to typical scopes in the program, differing mostly in some details and corner cases.
 * MIR scope tree is generated per mir function, during the creation of it.
 */

#pragma once

#include <base/ref.hpp>
#include <base/stable_container.hpp>

namespace compiler::mir {
    
    struct LifetimeScopeTree final {
        /**
        * MIR Lifetime scope.
        */
        struct LifetimeScope final{
            MCRef<LifetimeScope> parent;
            u64 depth;
            
            /**
             * Unique id, for mapping, comparision, etc.
             */
            u64 id;
            
            bool operator==(const LifetimeScope& other) const {
                return id == other.id;
            }
        };
        
        base::StableVector<const LifetimeScope> scopes;
        CRef<LifetimeScope> root;
       
        LifetimeScopeTree();

        CRef<LifetimeScope> newScope(CRef<LifetimeScope> parent);

        bool operator==(const LifetimeScopeTree& other) const;
    };

    using ScopeRef = CRef<LifetimeScopeTree::LifetimeScope>;

    /**
     * Returns a scope that is above all other scopes.
     */
     ScopeRef getSuperRootScope();

}
