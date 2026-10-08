// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

// Standardised OS, platform-family, compiler and architecture names.
//
// The OS_* macros name operating systems; PLATFORM_POSIX is the family flag.
//
// The raw macros are inconsistent across compilers (WIN32 vs _WIN32,
// __unix__ missing on Apple, __x86_64__ vs _M_X64 on MSVC), so our code
// should use these instead. All are always defined to 0 or 1.
//
// This header lives in base/config rather than os_utils on purpose:
// base is the lowest level of the dependency graph, so every module can
// use these names, including leaf modules like filepath_utils that must
// not depend on os_utils. It follows the pattern of build_type.hpp,
// which standardises the build-type names the same way.
//
// The IS_TARGET_* constants work with if constexpr only when both branches
// compile on every platform. When a branch calls an API that exists on one
// platform only (e.g. memfd_create), keep the #if BASE_TARGET_* form.
//
// Each macro is followed by its base::IS_TARGET_* counterpart; keep the
// pair together. The macros are global regardless of the namespace below.

#if defined(__APPLE__)
	#include <TargetConditionals.h>
#endif

namespace base {

	// --- Operating Systems ---
	// OS_* names an operating system; PLATFORM_POSIX below is the POSIX-family flag,
	// true on any of the OS_* targets that are POSIX systems.

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
	#define BASE_TARGET_OS_WINDOWS 1
#else
	#define BASE_TARGET_OS_WINDOWS 0
#endif
	constexpr bool IS_TARGET_OS_WINDOWS = BASE_TARGET_OS_WINDOWS;

	// __APPLE__ is true on all Apple platforms, not only on macOS
#if defined(TARGET_OS_OSX) && TARGET_OS_OSX
	#define BASE_TARGET_OS_MACOS 1
#else
	#define BASE_TARGET_OS_MACOS 0
#endif
	constexpr bool IS_TARGET_OS_MACOS = BASE_TARGET_OS_MACOS;

#if defined(__linux__) && !defined(__ANDROID__)
	#define BASE_TARGET_OS_LINUX 1
#else
	#define BASE_TARGET_OS_LINUX 0
#endif
	constexpr bool IS_TARGET_OS_LINUX = BASE_TARGET_OS_LINUX;

	// macOS is POSIX but does not define __unix__
#if defined(__unix__) || defined(__unix) || defined(__APPLE__)
	#define BASE_TARGET_PLATFORM_POSIX 1
#else
	#define BASE_TARGET_PLATFORM_POSIX 0
#endif
	constexpr bool IS_TARGET_PLATFORM_POSIX = BASE_TARGET_PLATFORM_POSIX;


	// --- Compilers ---

#if defined(__clang__)
	#define BASE_TARGET_COMPILER_CLANG 1
#else
	#define BASE_TARGET_COMPILER_CLANG 0
#endif
	constexpr bool IS_TARGET_COMPILER_CLANG = BASE_TARGET_COMPILER_CLANG;

	// When using Clang on Windows in MSVC-compatibility mode (clang-cl),
	// Clang defines _MSC_VER in addition to __clang__
#if defined(_MSC_VER) && !defined(__clang__)
	#define BASE_TARGET_COMPILER_MSVC 1
#else
	#define BASE_TARGET_COMPILER_MSVC 0
#endif
	constexpr bool IS_TARGET_COMPILER_MSVC = BASE_TARGET_COMPILER_MSVC;

	// clang defines __GNUC__ for compatibility
#if defined(__GNUC__) && !defined(__clang__)
	#define BASE_TARGET_COMPILER_GCC 1
#else
	#define BASE_TARGET_COMPILER_GCC 0
#endif
	constexpr bool IS_TARGET_COMPILER_GCC = BASE_TARGET_COMPILER_GCC;


	// --- Architectures ---

#if defined(__x86_64__) || defined(__x86_64) || defined(__i386__) || defined(__i386) \
	|| defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86)
	#define BASE_TARGET_ARCH_X86 1
#else
	#define BASE_TARGET_ARCH_X86 0
#endif
	constexpr bool IS_TARGET_ARCH_X86 = BASE_TARGET_ARCH_X86;

#if defined(__arm__) || defined(__thumb__) || defined(_M_ARM) || defined(__aarch64__) \
	|| defined(_M_ARM64)
	#define BASE_TARGET_ARCH_ARM 1
#else
	#define BASE_TARGET_ARCH_ARM 0
#endif
	constexpr bool IS_TARGET_ARCH_ARM = BASE_TARGET_ARCH_ARM;


	// --- Bit Widths ---

#if defined(_WIN64) || defined(__LP64__) || defined(_LP64)                             \
	|| (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8) || defined(__x86_64__) \
	|| defined(_M_X64) || defined(_M_AMD64) || defined(__aarch64__) || defined(_M_ARM64)
	#define BASE_TARGET_ARCH_64 1
	#define BASE_TARGET_ARCH_32 0
#elif defined(_WIN32) || defined(__ILP32__) || defined(_ILP32)                       \
	|| (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4) || defined(__i386__) \
	|| defined(_M_IX86) || defined(__arm__) || defined(_M_ARM)
	#define BASE_TARGET_ARCH_64 0
	#define BASE_TARGET_ARCH_32 1
#else
	#error "Target bit-width (pointer size) could not be determined."
#endif
	constexpr bool IS_TARGET_ARCH_64 = BASE_TARGET_ARCH_64;
	constexpr bool IS_TARGET_ARCH_32 = BASE_TARGET_ARCH_32;


	// --- Sanity Checks ---

	// Compilers
#if (BASE_TARGET_COMPILER_CLANG + BASE_TARGET_COMPILER_MSVC + BASE_TARGET_COMPILER_GCC) == 0
	#error "Unknown or unsupported compiler."
#elif (BASE_TARGET_COMPILER_CLANG + BASE_TARGET_COMPILER_MSVC + BASE_TARGET_COMPILER_GCC) > 1
	#error "Multiple compilers detected simultaneously."
#endif

	// Operating Systems
	// note: BASE_TARGET_PLATFORM_POSIX is deliberately left out as it is a family of operating systems
#if (BASE_TARGET_OS_WINDOWS + BASE_TARGET_OS_MACOS + BASE_TARGET_OS_LINUX) == 0
	#error "Unknown or unsupported target operating system."
#elif (BASE_TARGET_OS_WINDOWS + BASE_TARGET_OS_MACOS + BASE_TARGET_OS_LINUX) > 1
	#error "Multiple target operating systems detected simultaneously."
#endif

	// CPU Architectures
#if (BASE_TARGET_ARCH_X86 + BASE_TARGET_ARCH_ARM) == 0
	#error "Unknown or unsupported target CPU architecture."
#elif (BASE_TARGET_ARCH_X86 + BASE_TARGET_ARCH_ARM) > 1
	#error "Multiple target CPU architectures detected simultaneously."
#endif
}  // namespace base
