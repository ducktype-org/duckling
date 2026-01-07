#pragma once

namespace compiler::frontend  {

    /**
     * This flag indicates whether module modifier feature is enabled.
     * For use module modifier only make sense in language server mode.
     * This flag enables additional checks for dangling references in dev mode.
     */
    extern constinit bool use_module_modifier;
}