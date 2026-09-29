if(USE_REPLXX)
    set(vm_debugger_cli_impl UI/CLI/replxx/cli.cpp)
else()
    set(vm_debugger_cli_impl UI/CLI/minimal/cli.cpp)
endif()

add_files(debugger.cpp
        mapper.cpp
        UI/debug_adapter/debug_adapter.cpp
        UI/CLI/cli.cpp
        ${vm_debugger_cli_impl})

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
