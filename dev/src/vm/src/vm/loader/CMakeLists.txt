add_files(loader.cpp
          parser/elements.cpp
          parser/parser.cpp
          compiler/compiler.cpp
          compiler/type_builder.cpp
          )

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
