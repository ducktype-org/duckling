add_files(
    builtin_types.cpp
    builders/builders.cpp
    builders/instruction_builder.cpp
    serializer/serializer.cpp
    builtin_types.cpp
    builders/function_validator.cpp
)
set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
