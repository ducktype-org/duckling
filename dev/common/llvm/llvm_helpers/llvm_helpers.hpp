#pragma once

enum class Compiler { Clang, GCC };

#if defined(__clang__)
constexpr Compiler BuildCompiler = Compiler::Clang;
	#define LLVM_INCLUDE_BEGIN()                                                      \
		_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wshadow\"") \
			_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")
#elif defined(__GNUC__) || defined(__GNUG__)
constexpr Compiler BuildCompiler = Compiler::GCC;
	#define LLVM_INCLUDE_BEGIN()                                                            \
		_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wshadow=local\"") \
			_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")
#elif defined(_MSC_VER)
	#error "LLVM_INCLUDE_BEGIN does not support MSVS yet"
#endif

#define LLVM_INCLUDE_END() _Pragma("GCC diagnostic pop")
