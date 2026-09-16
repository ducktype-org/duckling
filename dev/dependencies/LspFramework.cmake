include(FetchContent)

set(LSP_FRAMEWORK_TAG "87c2f29d351048296ec6782fe29efbffde8aea8b")  # master, commited 2026-09-05

set(LSP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(lsp-framework
    GIT_REPOSITORY https://github.com/leon-bckl/lsp-framework.git
    GIT_TAG        ${LSP_FRAMEWORK_TAG}
    SYSTEM
)
FetchContent_MakeAvailable(lsp-framework)

# Suppress warnings in lsp-framework sources that conflict with our strict -Werror flags.
foreach(target lspgen lsp)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_compile_options(${target} PRIVATE
			-Wno-conversion -Wno-shadow -Wno-shadow=local -Wno-shadow=compatible-local
			-Wno-error=conversion -Wno-error=shadow=local -Wno-error=shadow=compatible-local
		)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
		target_compile_options(${target} PRIVATE
			-Wno-conversion -Wno-shadow
			-Wno-error=conversion -Wno-error=shadow
		)
	else()
		message(FATAL_ERROR "Error: UNKNOWN COMPILER")
	endif()
endforeach()
