add_files(loader.cpp
          parser/elements.cpp
          parser/parser.cpp
          compiler/compiler.cpp)

set(vm_sources ${vm_sources} ${files} PARENT_SCOPE)
