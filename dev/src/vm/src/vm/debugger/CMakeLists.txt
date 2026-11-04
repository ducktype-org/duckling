if(USE_REPLXX)
    set(vm_debugger_cli_impl UI/CLI/replxx.cpp)
else()
    set(vm_debugger_cli_impl UI/CLI/minimal.cpp)
endif()

add_files(debugger.cpp
        mapper.cpp
        UI/debug_adapter/debug_adapter.cpp
        UI/CLI/cli.cpp
        UI/CLI/common.cpp
        ${vm_debugger_cli_impl})

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
