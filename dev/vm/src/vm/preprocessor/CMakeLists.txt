add_files(preprocessor.cpp
          parser/elements.cpp
          parser/parser.cpp
          compiler/compiler.cpp
          validator/validator.cpp
          validator/detail/stack_state.cpp)

set(vm_sources ${vm_sources} ${files} PARENT_SCOPE)
