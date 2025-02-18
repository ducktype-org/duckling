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
 * @note for technical reasons the macro should be used at most once per line.
 */
#define REGISTER_FUNC_FOR_DEINIT(function_name) \
    namespace { \
        int CONCAT_2(deinit_helper_JG8MG9, __LINE__) = []() noexcept -> int { ::deinit::registerForDeinit(function_name); return 0; }();           \
    }

#if __cplusplus >= 202'600L
    #warning "@C++26 use the `_` anonymous variable in above macro. Note that it does not work now as it does in defer. I believe it should work under `a variable with automatic storage duration` case, but I'm not 100% sure."
#endif
