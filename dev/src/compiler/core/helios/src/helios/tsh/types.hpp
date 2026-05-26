/**
 * @file types.hpp
 * @brief Interfaces of the simpler kinds of types.
 *
 * The interfaces are not aware of the private implementation hierarchy in any way other than its
 * existence and name.
 */

#pragma once
#include "abstract_type.hpp"
#include "symbol_type.hpp"

#include <helios/symbols/symbol_abi.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <base/collections/optional.hpp>
#include <base/types/bits_and_bytes.hpp>

namespace compiler::tsh {
	class UnitAbstractTypeImpl;
	class VoidAbstractTypeImpl;
	class ByteAbstractTypeImpl;
	class BoolAbstractTypeImpl;
	class CharAbstractTypeImpl;
	class IntegralAbstractTypeImpl;
	class FloatAbstractTypeImpl;
	class RawPointerAbstractTypeImpl;
	class PointerAbstractTypeImpl;
	class StringAbstractTypeImpl;
	class TupleAbstractTypeImpl;
	class FunctionAbstractTypeImpl;
	class DynamicArrayAbstractTypeImpl;
	class StaticArrayAbstractTypeImpl;
	class VariantAbstractTypeImpl;
	class ClassAbstractTypeImpl;
	class NamespaceAbstractTypeImpl;
	class ModuleAbstractTypeImpl;
	class MetaAbstractTypeImpl;
	class ImportAbstractTypeImpl;
	class TypeTemplateAbstractTypeImpl;

	/******************\
	|    BASIC TYPES   |
	\******************/

	/**
	 * @brief The Unit type.
	 *
	 * The Unit type is the type with only one value -- the empty tuple ().
	 */
	class UnitAbstractType final: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(UnitAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(UnitAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(UnitAbstractType)
	};

	/**
	 * @brief The Void type.
	 *
	 * The Void type has no values. It is uninstantiable.
	 * This can be used to mark functions which must never return.
	 *
	 * The Void type is a subtype of every type.
	 * While this may seem strange, it is useful in generic variance contexts.
	 */
	class VoidAbstractType final: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(VoidAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(VoidAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(VoidAbstractType)
	};

	/**
	 * @brief The Byte type.
	 *
	 * A value of the Byte type holds exactly one byte.
	 * It may be desirable to use a type which represents exactly a byte
	 * instead of an Integral of size 8 or a Char when it is particularly
	 * important that the underlying data is a byte, e.g. in cryptography.
	 */
	class ByteAbstractType final: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(ByteAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(ByteAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(ByteAbstractType)
	};

	/**
	 * @brief The Bool type.
	 *
	 * Despite having a size of 8 bits, a value of the Bool type can be
	 * only one of two values: true (1) and false (0).
	 */
	class BoolAbstractType final: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(BoolAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(BoolAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(BoolAbstractType)
	};

	/**
	 * @brief The Char type.
	 *
	 * A value of this type represents a character.
	 * It is not decided how encodings other than ASCII will be supported yet.
	 */
	class CharAbstractType final: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(CharAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(CharAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(CharAbstractType)
	};

	/**
	 * @brief The Integral types.
	 *
	 * Each Integral type has a size which is a power of two between 8 and 128 (inclusive).
	 * Additionally, it can be signed or unsigned.
	 */
	class IntegralAbstractType final: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(IntegralAbstractType, AbstractType)

		enum class Signedness : bool { Signed = true, Unsigned = false };

		/**
		 * @brief Get the size of the integral type.
		 * @return The size of the type.
		 */
		[[nodiscard]]
		Bits getSize() const;

		/**
		 * @return true if the integer is singed
		 */
		[[nodiscard]]
		Signedness getSignedness() const;

		CONSTRUCT_WITH_CHECKED_CAST(IntegralAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(IntegralAbstractType)
	};

	/**
	 * @brief The Float types.
	 *
	 * The Float types come in sizes being powers of two between 16 and 128 (inclusive),
	 * as well as 80 bits.
	 */
	class FloatAbstractType final: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(FloatAbstractType, AbstractType)

		/**
		 * @brief Get the size of the floating point type.
		 * @return The size of the type.
		 */
		[[nodiscard]]
		Bits getSize() const;

		CONSTRUCT_WITH_CHECKED_CAST(FloatAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(FloatAbstractType)
	};

	/*******************\
	|   POINTER TYPES   |
	\*******************/

	/**
	 * @brief The Raw Pointer type.
	 *
	 * A value of this type is simply a memory address.
	 */
	class RawPointerAbstractType final: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(RawPointerAbstractType, AbstractType)

		[[nodiscard]]
		bool isMutable() const;

		CONSTRUCT_WITH_CHECKED_CAST(RawPointerAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(RawPointerAbstractType)
	};

	/**
	 * @brief The (typed) Pointer types.
	 *
	 * A value of this type is simply a memory address.
	 * However, it is statically known what the type of the pointee is.
	 */
	class PointerAbstractType final: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(PointerAbstractType, AbstractType)

		/**
		 * @brief Gets the underlying symbol type of the Pointer type.
		 * @return The underlying symbol type of the Pointer type.
		 */
		[[nodiscard]]
		SymbolType<> getPointee() const;

		/**
		 * @brief Gets the underlying type of the Pointer type.
		 * @return The underlying type.
		 */
		[[nodiscard]]
		AbstractType getUnderlyingType() const;

		CONSTRUCT_WITH_CHECKED_CAST(PointerAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(PointerAbstractType)
	};

	/*******************\
	|  COMPOSITE TYPES  |
	\*******************/

	/**
	 * @brief The Tuple types.
	 *
	 * Each Tuple type is simply a tuple of elements, with additional specification
	 * about whether these elements are mutable or not.
	 *
	 * For example, (A, B) and (A, mut B) are two different tuple types.
	 * Nota bene, the former is implicitly coercible to the latter.
	 */
	class TupleAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(TupleAbstractType, AbstractType)

		[[nodiscard]]
		const std::vector<SymbolType<>>& getComponents() const;

		[[nodiscard]]
		std::vector<AbstractType> getComponentAbstractTypes() const;

		CONSTRUCT_WITH_CHECKED_CAST(TupleAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(TupleAbstractType)
	};

	/**
	 * @brief The Function types.
	 *
	 * Each function type has a list of parameter types and a return type.
	 * Additionally, it may be a pure function and it may be a free function.
	 *
	 * While function parameter types and return types are rather self explanatory,
	 * purity and freedom are not.
	 *
	 * A function is **pure** if it has no side effects. That means, that calling it
	 * multiple times with the same arguments will always yield the same result
	 * and the same global state of the program.
	 *
	 * Marking a function pure is useful
	 * for functional programming, but is not much more than a way for the user
	 * to protect themselves from themselves. The compiler can assist the user by
	 * checking if the user's purity expectations are satisfied.
	 *
	 * By default, a function is not pure, because Duckling is a predominantly imperative
	 * language and use of global values and shared states is expected. Also, it's
	 * easier to introduce a `pure` annotation than a `nonpure` annotation.
	 *
	 * A function is **free** if it is not associated with any object. This object
	 * may be an instance of a class, where the function is a method of said class,
	 * or it can be a structure with captures and bindings.
	 *
	 * A function that is free is, in effect, a C-like function pointer, while a
	 * non-free function can be thought of as a function object, like a lambda with
	 * captures or a partially applied function.
	 *
	 * A non-free (bound? [to an object]) function can be represented as two pointers:
	 * one to the executable code, and another to a structure with captures and bindings.
	 * A non-free function cannot be converted to a free function, but a free function can
	 * be trivially converted to a function object by supplying it with a pointer to an empty
	 * structure.
	 *
	 * A non-free function introduces overhead when being called or passed to higher order
	 * functions. Yet we expect that if a function is expected, then a lambda with captures
	 * will work in that context. Thus, a function is by default non-free, and can be marked
	 * as free in order to constrict expressive power, but enable optimisations.
	 *
	 * A function can be non-free and pure if it does not change its captures.
	 */
	class FunctionAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(FunctionAbstractType, AbstractType)

		/**
		 * @brief Gets the parameter types of the function type.
		 * @return The parameter types of the function type.
		 */
		[[nodiscard]]
		const std::vector<SymbolType<>>& getParameterTypes() const;

		/**
		 * @brief Gets the result type of the function type.
		 * @return The result type of the function type.
		 */
		[[nodiscard]]
		SymbolType<> getResultType() const;

		/**
		 * @brief Check whether the function type is pure or not.
		 * @return Whether the function type is pure or not.
		 */
		[[nodiscard]]
		bool isPure() const;

		/**
		 * @brief Check whether the function type is free or not.
		 * @return Whether the function type is free or not.
		 */
		[[nodiscard]]
		bool isFree() const;

		CONSTRUCT_WITH_CHECKED_CAST(FunctionAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(FunctionAbstractType)
	};

	class DynamicArrayAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(DynamicArrayAbstractType, AbstractType)

		/**
		 * @brief Gets the type of the elements of the dynamic array.
		 * @return The type of the elements of the dynamic array.
		 */
		[[nodiscard]]
		SymbolType<> getElementType() const;

		CONSTRUCT_WITH_CHECKED_CAST(DynamicArrayAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(DynamicArrayAbstractType)
	};

	class StaticArrayAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(StaticArrayAbstractType, AbstractType)

		/**
		 * @brief Gets the type of the elements of the dynamic array.
		 * @return The type of the elements of the dynamic array.
		 */
		[[nodiscard]]
		SymbolType<> getElementType() const;

		/**
		 * @brief Gets the element count of the static array.
		 * @return The number of elements in the static array.
		 */
		[[nodiscard]]
		usize getSize() const;

		CONSTRUCT_WITH_CHECKED_CAST(StaticArrayAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(StaticArrayAbstractType)
	};

	/**
	 * @brief The Variant types.
	 *
	 * The value of a variant type is (conceptually) equal to a value of
	 * exactly one of its component types. In practice, we mark the type of the
	 * dynamic value using additional discriminatory bytes, or more cleverly if possible.
	 */
	class VariantAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(VariantAbstractType, AbstractType)

		[[nodiscard]]
		const std::vector<SymbolType<>>& getUnderlyingTypes() const;

		[[nodiscard]]
		SymbolType<> getMember(usize index) const;

		CONSTRUCT_WITH_CHECKED_CAST(VariantAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(VariantAbstractType)
	};

	/**
	 * @brief The Class types.
	 *
	 * This kind of types is by far the most complex in implementation.
	 *
	 * A Class type may be cyclically dependent on itself, which is why construction
	 * requires only a Symbol ID (which then leads to the place of definition in the PST,
	 * whence all required AbstractTypermation is gathered).
	 *
	 * A Class may have zero or one base classes and may implement arbitrarily many interfaces.
	 * A Class may define its own member fields and member functions. All of the above can be
	 * accessed via ClassAbstractType methods.
	 */
	class ClassAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(ClassAbstractType, AbstractType)

		/**
		 * Gets the SymID of the class type.
		 * @return The SymID of the class type.
		 */
		[[nodiscard]]
		compiler::helios::SymID getSymbol() const;

		/**
		 * @brief Returns the ABI of this class, derived from its `extern` specifiers.
		 *
		 * Returns `DefaultAbi` for plain classes and `CAbi` for classes annotated
		 * with `extern("C")`.
		 */
		[[nodiscard]]
		compiler::helios::SymbolABI getABI(query::Context& ctx) const;

		/**
		 * Gets the type of the base class.
		 * @param ctx The Query Context necessary to refer to the definition of the class.
		 * @return The type of the base class.
		 */
		[[nodiscard]]
		base::Optional<ClassAbstractType> getBaseClassType(query::Context& ctx) const;

		/**
		 * Gets the symbol of the base class.
		 * @param ctx The Query Context necessary to refer to the definition of the class.
		 * @return The symbol of the base class.
		 */
		[[nodiscard]]
		base::Optional<compiler::helios::SymID> getBaseClassSymbol(query::Context& ctx) const;

		/**
		 * Gets the symbols of implemented interfaces.
		 * @param ctx The Query Context necessary to refer to the definition of the class.
		 * @return The symbols of implemented interfaces.
		 */
		[[nodiscard]]
		std::vector<ClassAbstractType> getImplementedInterfaceTypes(query::Context& ctx) const;
		// @TODO: change return type to InterfaceAbstractType when interface type is created.

		[[nodiscard]]
		std::vector<compiler::helios::SymID> getImplementedInterfaceSymbols(query::Context& ctx
		) const;

		/**
		 * Gets the type of a member.
		 * @param sym The member, the type of which is requested.
		 * @param ctx The Query Context necessary to refer to the definition of the class.
		 * @return The type of the member.
		 */
		[[nodiscard]]
		SymbolType<> getMemberType(compiler::helios::SymID sym, query::Context& ctx) const;

		CONSTRUCT_WITH_CHECKED_CAST(ClassAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(ClassAbstractType)
	};

	/***********************\
	|  MISCELLANEOUS TYPES  |
	\***********************/

	class StringAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(StringAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(StringAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(StringAbstractType)
	};

	class NamespaceAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(NamespaceAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(NamespaceAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(NamespaceAbstractType)
	};

	class ModuleAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(ModuleAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(ModuleAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(ModuleAbstractType)
	};

	// The Meta type is the type which hold values being types.
	// One could write, for example, `type myInt64 = i64;`.
	// Might be useful for generic interfaces and compile time code execution.
	class MetaAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(MetaAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(MetaAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(MetaAbstractType)
	};

	class ImportAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(ImportAbstractType, AbstractType)

		CONSTRUCT_WITH_CHECKED_CAST(ImportAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(ImportAbstractType)
	};

	/**
	 * @brief The Type Template type.
	 *
	 * A Type Template represents an "incomplete" type.
	 *
	 * This is a compile-time concept used primarily to implement generics.
	 * For instance, the bare keyword `List` is represented as a Type Template which cannot hold
	 * values or exist in the final program's memory on its own, indexing it with a type (e.g.,
	 * `List[i64]`) "saturates" the template, resulting in a concrete `DynamicArrayAbstractType`.
	 *
	 * The source of a template defines its behavior:
	 * - **BuiltinKind**: Identifies primitive generic constructors recognized
	 *   directly by the compiler (like the built-in dynamic list).
	 * - **SymID**: A reference to a user-defined generic class.
	 *
	 * @TODO: #717 This may come in handy when implementing generics/templates. This implementation
	 * may change then. This is just a stub needed for generic List implementation.
	 */
	class TypeTemplateAbstractType: public AbstractType {
	public:
		SETUP_TYPE_WITH_BASE(TypeTemplateAbstractType, AbstractType)

		enum class BuiltinKind {
			List  ///< The built-in dynamic array constructor.
		};

		/**
		 * @brief The origin of the template/generic.
		 */
		using Source = std::variant<BuiltinKind, helios::SymID>;

		/**
		 * @brief Gets the source (built-in or user-defined) of this template.
		 * @return The template source.
		 */
		[[nodiscard]] Source getSource() const;

		/**
		 * @brief Creates a concrete type from this type template instantiated with the
		 * `element_type`.
		 * @param element_type The type to instantiate the type template with.
		 * @return The concrete type after instantiation.
		 */
		[[nodiscard]] AbstractType instantiate(
			query::Context& ctx, const SymbolType<>& element_type
		) const;


		CONSTRUCT_WITH_CHECKED_CAST(TypeTemplateAbstractType)

		CONSTRUCT_FROM_IMPLEMENTATION(TypeTemplateAbstractType)
	};
}
