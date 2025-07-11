#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <query_framework/context_fd.hpp>
#include <string>

namespace compiler::driver {


    /**
     * Compiles given module to assembly code and returns it as string.
     * @note For now this is implemented as a standalone function, in the future
     * it might depend on some cached query.
     */
    std::string compileModuleToAssembly(
        query::Context& ctx,
        frontend::ModuleID module_id
    );

}
