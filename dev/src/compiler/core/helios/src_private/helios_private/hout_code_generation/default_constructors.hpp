#pragma once


#include <helios/hout/hout.hpp>
#include <typesystem/higher/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::houtgen {
	/**
	 * @brief Get the compiler-generated HOUT representation of the implicit constructor for a class.
	 *
	 * The implicit constructor is a function that takes parameters for each field of the class
	 * and returns an instance of the class with those fields initialised accordingly.
	 * @note This constructor is used in `var a: T = T(x, y, z)` cases and requires the uninitialized
	 * class fields to be passed as parameters.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryClassConstructor, tsh::ClassAbstractType, CRef<query::QResult<HOUTFunction>>, ({})
	);

	/**
	 * @brief Get the compiler-generated HOUT representation of the default constructor for a class.
	 *
	 * The default constructor is a function that doesn't take any parameters and initializes a
	 * class with the fields initial value or a default value if the initial value for a field was
	 * not provided. This handles cases like:
	 * ```
	 * class A { p: i32; }
	 * class B { x: i32 = 2; y: i64; arr: B[5]; }
	 *
	 * # B's default constructor is called here. Initializes the `x` field with the
	 * # initial value of 2, `y` with the default value for `i64` - 0, and `arr` by the value
	 * returned # by the default constructor of the `B[5]` static array. var x: B;
	 * ```
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryDefaultClassConstructor,
		tsh::ClassAbstractType,
		CRef<query::QResult<HOUTFunction>>,
		({})
	);

	/**
	 * @brief Get the compiler-generated HOUT representation of the default constructor for a static
	 * array.
	 *
	 * The default constructor initializes each element of the array to its default value calling
	 * other default constructors if needed. This handles cases like: `var arr: SomeClass[5];`.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryDefaultStaticArrayConstructor,
		tsh::StaticArrayAbstractType,
		CRef<query::QResult<HOUTFunction>>,
		({})
	);

	/**
	 * @brief Get the hout expression which default initializes a variable of a given type.
	 * This could either be:
	 * - `code::DefaultValueExpr` - for primitives. This means zero-initialization of the variable
	 * in LLVM.
	 * - `code::CallExpr` - for classes/arrays/tuples. This is a call expression to the default
	 * constructor of the given type.
	 * - `code::ListInitExpr` - for dynamic arrays. This maps to a call to the builtin ListInit
	 * function in LLVM.
	 *
	 * Logs an error if the type cannot be default initialized (f.e. is a ref/box/unit).
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryDefaultInitializerExpr, tsh::SymbolType<>, CRef<query::QResult<Box<code::Expr>>>, ({})
	);
}
