add_files(
        supervisor.cpp
        vcpu.cpp
)

set(riftvm_sources ${riftvm_sources} ${files} PARENT_SCOPE)
