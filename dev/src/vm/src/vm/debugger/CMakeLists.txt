add_files(debugger.cpp
          UI/debug_adapter/debug_adapter.cpp
          UI/CLI/cli.cpp)

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
