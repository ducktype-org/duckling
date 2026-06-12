add_files(
    bytecode.cpp
    builtin_types.cpp
    const_value.cpp
    builders/instruction_builder.cpp
    serializer/serializer.cpp
    validator/function_validator.cpp
    validator/initial_value.cpp
    validator/local_stack_database.cpp
    validator/local_stack_database_builder.cpp
    validator/type_validator.cpp
    validator/valid_type/type_context.cpp
    validator/valid_type/valid_type.cpp
    validator/valid_program.cpp
)
set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
