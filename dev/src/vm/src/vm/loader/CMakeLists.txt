add_files(loader.cpp
          parser/elements.cpp
          parser/parser.cpp
          compiler/ivm_compiler.cpp
          compiler/safe/safe_compiler.cpp
          compiler/safe/type_builder.cpp
          )

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
