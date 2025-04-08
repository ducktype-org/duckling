add_files(
    builders/builders.cpp
    builders/instruction_builder.cpp
    serializer/serializer.cpp
)
set(vm_sources ${vm_sources} ${files} PARENT_SCOPE)
