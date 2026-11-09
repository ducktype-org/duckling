// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/config/target_info.hpp>

#if BASE_TARGET_COMPILER_CLANG
	#define LLVM_INCLUDE_BEGIN()                                                              \
		_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wshadow\"")         \
			_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")                          \
				_Pragma("GCC diagnostic ignored \"-Wconversion\"")                            \
					_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")           \
						_Pragma("GCC diagnostic ignored \"-Wunnecessary-virtual-specifier\"") \
							_Pragma("GCC diagnostic ignored \"-Wcpp\"")
#elif BASE_TARGET_COMPILER_GCC
	#define LLVM_INCLUDE_BEGIN()                                                            \
		_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wshadow=local\"") \
			_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")                        \
				_Pragma("GCC diagnostic ignored \"-Wconversion\"")                          \
					_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif BASE_TARGET_COMPILER_MSVC
	#error "LLVM_INCLUDE_BEGIN does not support MSVC yet"
#endif

#define LLVM_INCLUDE_END() _Pragma("GCC diagnostic pop")
