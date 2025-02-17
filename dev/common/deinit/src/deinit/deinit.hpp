#pragma once

#include <functional>
#include <base/define_helper.hpp>

namespace deinit {
    /**
     * @brief Registers a function to be called after main.
     */
    void registerForDeinit(std::function<void()>);
}

/**
 * Helper macro that can be used as:
 * REGISTER_FUNC_FOR_DEINIT(function_name);
 * in global scope to register function_name to be called after main.
 * @important This macro should be used in cpp files, to avoid duplication.
 */
#define REGISTER_FUNC_FOR_DEINIT(function_name) \
    namespace { \
        PUSH_DIAGNOSTIC _Pragma("GCC diagnostic ignored \"-Wc++26-extensions\"")\
        int _ =  { []() noexcept -> int { ::deinit::registerForDeinit(function_name); return 0; } };           \
        POP_DIAGNOSTIC \
    }

#if __cplusplus >= 202'600L
    #warning "Remove the pragmas above when upgrading to C++26"
#endif