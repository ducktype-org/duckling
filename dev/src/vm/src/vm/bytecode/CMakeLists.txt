add_files(
    builtin_types.cpp
    builders/instruction_builder.cpp
    serializer/serializer.cpp
    validator/function_validator.cpp
    validator/type_validator.cpp
    validator/type/type_context.cpp
    validator/type/type.cpp
    validator/valid_program.cpp
)
set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
