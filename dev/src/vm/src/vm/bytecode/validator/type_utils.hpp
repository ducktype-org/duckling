#pragma once

#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type_context.hpp>

namespace vm::code::detail {
	using namespace vm::code;
	using FieldVector = std::vector<std::pair<base::StrID, vm::TypeRef>>;

	template<typename T>
	concept InheritableTypeConcept
		= std::is_same_v<T, ClassType> || std::is_same_v<T, InterfaceType>;
	template<typename T>
	concept FieldableTypeConcept = std::is_same_v<T, ClassType> || std::is_same_v<T, DataType>;
	template<typename F>
	concept ErrorFactoryConcept
		= std::invocable<F> && std::is_base_of_v<ValidationError, std::invoke_result_t<F>>;
	template<typename T>
	concept TypeOfDataConcept = std::is_constructible_v<TypeOfData, T>;

/**
 * @brief Retrieves a type with a given name from the `TypeContext` and checks if it has an
 * expected type. If yes, it retrieves this type from the variant and returns it. If not, throws
 * an error given by the error_factory function.
 *
 * @param ctx type context to retrieve the type from.
 * @param name name of the type to retrieve.
 * @param context_for_error a type needed to throw the UnknownSubtypeError. This is the type for
 * which subtype we're looking for.
 * @param error_factory a function which returns the error to be thrown in case of type
 * mismatch. For example, if a type specified by the name in clazz.extends is not a ClassType,
 * the error_factory should return an InvalidExtendsError.
 *
 * @note This function causes a dangling reference warning, which I strongly believe is a false
 * positive, thus the pragmas.
 */
#if defined(__GNUG__) || defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif
	template<TypeOfDataConcept ExpectedType, ErrorFactoryConcept ErrorFactory>
	const ExpectedType& getType(
		const TypeContext&  ctx,
		const base::StrID&  name,
		const TypeOfData&   context_for_error,
		const ErrorFactory& error_factory
	) {
		const TypeOfData& type_of_data
			= *ctx.getCurrentTypes().atMaybe(name).expect<UnknownSubtypeError>(
				context_for_error, name
			);
		if (const auto* specific_type = std::get_if<ExpectedType>(&type_of_data))
			return *specific_type;
		throw error_factory();
	}
#if defined(__GNUG__) || defined(__clang__)
	#pragma GCC diagnostic pop
#endif
}  // namespace vm::code::detail
