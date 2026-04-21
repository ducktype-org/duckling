#pragma once


#include <frontend/pst_parser/source_position_locked.hpp>
#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
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
	 * - `code::DefaultValueExpr` - for trivially zero-initializable types like primitives or
	 * aggregate types storing zero-initializable types. This then maps to `ZeroInitialize` in LLVM.
	 * - `code::LiteralUnitExpr` - for unit types.
	 * - `code::LiteralTypeExpr` storing a void type - for meta types.
	 * - `code::CallExpr` - for non-trivially-zero-initializable classes/arrays/tuples. This is a
	 * call expression to the default constructor of the given type.
	 *
	 * Expects the given type to be default initializable.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QueryDefaultInitializerExpr, tsh::SymbolType<>, CRef<query::QResult<Box<code::Expr>>>, ({})
	);


	/**
	 * @brief Get the hout expression which default initializes a variable of a given type and log
	 * an error if the type can't be default initialized.
	 * @return A HOUT Expression that initializes the given type, or an error if the type is not
	 * default constructible.
	 */
	query::QResult<CRef<code::Expr>> getDefaultInitializerExpr(
		query::Context& ctx, const tsh::SymbolType<>& type, pst::SourcePositionLocked pos
	);
}
