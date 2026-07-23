#pragma once

#if defined(__clang__)
	#define LLVM_INCLUDE_BEGIN()                                                              \
		_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wshadow\"")         \
			_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")                          \
				_Pragma("GCC diagnostic ignored \"-Wconversion\"")                            \
					_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")           \
						_Pragma("GCC diagnostic ignored \"-Wunnecessary-virtual-specifier\"") \
							_Pragma("GCC diagnostic ignored \"-W#warnings\"")
#elif defined(__GNUC__) || defined(__GNUG__)
	#define LLVM_INCLUDE_BEGIN()                                                            \
		_Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wshadow=local\"") \
			_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")                        \
				_Pragma("GCC diagnostic ignored \"-Wconversion\"")                          \
					_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#elif defined(_MSC_VER)
	#error "LLVM_INCLUDE_BEGIN does not support MSVS yet"
#endif

#define LLVM_INCLUDE_END() _Pragma("GCC diagnostic pop")
