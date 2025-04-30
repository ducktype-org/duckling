add_files(
    builtin_types.cpp
    builders/builders.cpp
    builders/instruction_builder.cpp
    serializer/serializer.cpp
    builtin_types.cpp
)
set(vm_sources ${vm_sources} ${files} PARENT_SCOPE)
