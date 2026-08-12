include(FetchContent)

set(LSP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(lsp-framework
    GIT_REPOSITORY https://github.com/leon-bckl/lsp-framework.git
    GIT_TAG        1.3.1
    GIT_SHALLOW    TRUE
    SYSTEM
)
FetchContent_MakeAvailable(lsp-framework)

foreach(target lspgen lsp)
    target_compile_options(${target} PRIVATE
        -Wno-error=shadow=local
        -Wno-shadow
        -Wno-error=conversion
        -Wno-conversion
    )
endforeach()
