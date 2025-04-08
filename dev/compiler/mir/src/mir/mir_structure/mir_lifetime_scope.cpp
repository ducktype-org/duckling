#include "mir_lifetime_scope.hpp"

namespace compiler::mir {

    namespace {
        LifetimeScope root_scope(nullptr, 0);
    }

    LifetimeScopeTree::LifetimeScopeTree(): root(&root_scope) {}

    CRef<LifetimeScope> LifetimeScopeTree::newScope(CRef<LifetimeScope> parent) {
        auto key = scopes.emplaceBack(parent, parent->depth + 1);
        return scopes.getCRef(key).value();
    }
}
