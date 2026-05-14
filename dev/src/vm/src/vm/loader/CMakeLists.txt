add_files(loader.cpp
          parser/elements.cpp
          parser/parser.cpp
          compiler/compiler.cpp
          compiler/safe/safe_compiler.cpp
          compiler/safe/type_builder.cpp
          compiler/fast/fast_compiler.cpp
          compiler/fast/type_builder.cpp
          compiler/fast/instruction_lowering.cpp
          )

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
