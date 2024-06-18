/**
 * \file type_info.hpp
 * \brief Interfaces of the simpler kinds of types.
 *
 * The interface is not aware of the internal implementation hierarchy in any way other than its
 * existence and name.
 */

#pragma once
#include "type_info.hpp"
#include "type_desc.hpp"
#include <helios/scope_symbol_id.hpp>
#include <base/optional.hpp>

namespace ts {
	namespace internal {
		class UnitInfoImpl;
		class VoidInfoImpl;
		class ByteInfoImpl;
		class BoolInfoImpl;
		class CharInfoImpl;
		class IntegralInfoImpl;
		class FloatInfoImpl;
		class RawPointerInfoImpl;
		class PointerInfoImpl;
		class ReferenceInfoImpl;
		class TupleInfoImpl;
		class FunctionInfoImpl;
		class VariantInfoImpl;
		class ClassInfoImpl;
		class NamespaceInfoImpl;
		class ModuleInfoImpl;
		class MetaInfoImpl;
	}

	/******************\
	|    BASIC TYPES   |
	\******************/

	/**
	 * \brief The Unit type.
	 *
	 * The Unit type is the type with only one value -- the empty tuple ().
	 */
	class UnitInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(UnitInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(UnitInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Unit)
	};

	/**
	 * \brief The Void type.
	 *
	 * The Void type has no values. It is uninstantiable.
	 * This can be used to mark functions which must never return.
	 *
	 * The Void type is a subtype of every type.
	 * While this may seem strange, it is useful in generic variance contexts.
	 */
	class VoidInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(VoidInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(VoidInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Void)
	};

	/**
	 * \brief The Byte type.
	 *
	 * A value of the Byte type holds exactly one byte.
	 * It may be desirable to use a type which represents exactly a byte
	 * instead of an Integral of size 8 or a Char when it is particularly
	 * important that the underlying data is a byte, e.g. in cryptography.
	 */
	class ByteInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(ByteInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(ByteInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Byte)
	};

	/**
	 * \brief The Bool type.
	 *
	 * Despite having a size of 8 bits, a value of the Bool type can be
	 * only one of two values: true (1) and false (0).
	 */
	class BoolInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(BoolInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(BoolInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Bool)
	};

	/**
	 * \brief The Char type.
	 *
	 * A value of this type represents a character.
	 * It is not decided how encodings other than ASCII will be supported yet.
	 */
	class CharInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(CharInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(CharInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Char)
	};

	/**
	 * \brief The Integral types.
	 *
	 * Each Integral type has a size which is a power of two between 8 and 128 (inclusive).
	 * Additionally, it can be signed or unsigned.
	 */
	class IntegralInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(IntegralInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(IntegralInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Integral)
	};

	/**
	 * \brief The Float types.
	 *
	 * The Float types come in sizes being powers of two between 16 and 128 (inclusive),
	 * as well as 80 bits.
	 */
	class FloatInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(FloatInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(FloatInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Float)
	};

	/*******************\
	|   POINTER TYPES   |
	\*******************/

	/**
	 * \brief A type supplied with mutability information.
	 *
	 * It's called "Component Type" because it is used in types which are composed of other types.
	 * For example, a typed pointer may point to an immutable value. Or a tuple may have some
	 * of its components locked in as immutable.
	 */
	struct ComponentType {
		/**
		 * \brief The actual type of the component.
		 */
		const TypeInfo type;

		/**
		 * \brief Whether the component is mutable or not.
		 */
		const bool is_mutable = false;

		/**
		 * \brief Create a string representation of the component type.
		 */
		[[nodiscard]]
		std::string toString() const;

		/**
		 * Check whether a component type is implicitly coercible to another component type.
		 * @param target The target component.
		 * @param ctx The query context required f
		 * @return
		 */
		[[nodiscard]]
		bool isImplicitlyCoercible(ComponentType target, query::detail::ContextType& ctx) const;

		[[nodiscard]]
		auto operator<=>(const ComponentType& other) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
	};

	/**
	 * \brief The Raw Pointer type.
	 *
	 * A value of this type is simply a memory address.
	 */
	class RawPointerInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(RawPointerInfo, TypeInfo)

		[[nodiscard]]
		bool isMutable() const;

		CONSTRUCT_WITH_CHECKED_CAST(RawPointerInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(RawPointer)
	};

	/**
	 * \brief The (typed) Pointer types.
	 *
	 * A value of this type is simply a memory address.
	 * However, it is statically known what the type of the pointee is.
	 */
	class PointerInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(PointerInfo, TypeInfo)

		/**
		 * \brief Gets the underlying component of the Pointer type.
		 * @return The underlying component of the Pointer type.
		 */
		[[nodiscard]]
		ComponentType getComponent() const;

		/**
		 * \brief Gets the underlying type of the Pointer type.
		 * \return The underlying type.
		 */
		[[nodiscard]]
		TypeInfo getUnderlyingType() const;

		/**
		 * \brief Checks whether the data under the pointer is mutable.
		 * \return Whether the data under the pointer is mutable.
		 */
		[[nodiscard]]
		bool isMutable() const;

		CONSTRUCT_WITH_CHECKED_CAST(PointerInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Pointer)
	};

	/**
	 * \brief The kind of a Reference type. See documentation of each kind for details.
	 */
	enum class ReferenceKind {
		/**
		 * \brief A reference of this kind owns its referee.
		 *
		 * When the reference is destroyed, so is the referee.
		 * The reference does not give access to explicit destruction of the referee, because
		 * destruction is automatic.
		 */
		BOX,

		/**
		 * \brief A reference of this kind specifically does **not** own its referee.
		 *
		 * When the reference is destroyed, the referee remains untouched.
		 * Additionally, the reference does not give access to explicit destruction of the referee.
		 * This kind of reference can only be constructed from a BOX or PTR reference.
		 */
		REF,

		/**
		 * \brief A reference of this kind allows the user to decide if they want to explicitly
		 * destroy the referee.
		 *
		 * When the reference is destroyed, the referee remains untouched.
		 * The reference gives access to explicit destruction of the referee.
		 * This kind of reference is somewhat unsafe, but necessary in e.g. cyclic data structures.
		 */
		PTR
	};

	/**
	 * \brief The Reference types.
	 *
	 * A value of a Reference type is just a memory address with an
	 * associated type, just like in the case of a (typed) Pointer.
	 * However, a reference carries additional semantics and guarantees,
	 * such as leakiness, nullability and uniqueness.
	 */
	class ReferenceInfo final: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(ReferenceInfo, TypeInfo)

		/**
		 * \brief Create a new reference type with the given parameters.
		 * \param underlying_type The underlying type of the reference.
		 * \param ref_kind The kind of a reference.
		 * \param leaking Whether the reference can leak its value.
		 * By default, references are non-leaking.
		 * \param nullable Whether the reference can be empty.
		 * By default, references cannot be empty.
		 * \param unique Whether the reference is unique.
		 * By default, references can be copied and shared.
		 * \return The created reference type.
		 */
		static ReferenceInfo create(
			TypeInfo      underlying_type,
			ReferenceKind ref_kind,
			bool          leaking  = false,
			bool          nullable = false,
			bool          unique   = false
		);

		/**
		 * \brief Gets the underlying type.
		 * \return The underlying type.
		 */
		[[nodiscard]]
		TypeInfo getUnderlyingType() const;

		/**
		 * \brief Gets the reference kind.
		 * \return The reference kind.
		 */
		[[nodiscard]]
		ReferenceKind getReferenceKind() const;

		/**
		 * \brief Check whether the reference is leaking or not.
		 * \return Whether the reference is leaking or not.
		 */
		[[nodiscard]]
		bool isLeaking() const;

		/**
		 * \brief Check whether the reference is nullable or not.
		 * \return Whether the reference is nullable or not.
		 */
		[[nodiscard]]
		bool isNullable() const;

		/**
		 * \brief Check whether the reference is unique or not.
		 * \return Whether the reference is unique or not.
		 */
		[[nodiscard]]
		bool isUnique() const;

		CONSTRUCT_WITH_CHECKED_CAST(ReferenceInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Reference)
	};

	/*******************\
	|  COMPOSITE TYPES  |
	\*******************/

	/**
	 * \brief The Tuple types.
	 *
	 * Each Tuple type is simply a tuple of elements, with additional specification
	 * about whether these elements are mutable or not.
	 *
	 * For example, (A, B) and (A, mut B) are two different tuple types.
	 * Nota bene, the former is implicitly coercible to the latter.
	 */

	class TupleInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(TupleInfo, TypeInfo)

		[[nodiscard]]
		const std::vector<ComponentType>& getComponents() const;

		CONSTRUCT_WITH_CHECKED_CAST(TupleInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Tuple)
	};

	/**
	 * \brief The Function types.
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
	 * By default, a function is not pure, because Rift is a predominantly imperative
	 * language and use of global values and shared states is expected. Also, it's
	 * easier to introduce a `pure` annotation than a `nonpure` annotation.
	 *
	 * A function is **free** if it is not associated with any object. This object
	 * may be an instance of a class, where the function is a method of said class,
	 * or it can be a structure with captures and bindings.
	 *
	 * A function that is free is, in effect, a C-like function pointer, while a
	 * non-free function can be thought of as a function object, lika a lambda with
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
	class FunctionInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(FunctionInfo, TypeInfo)

		/**
		 * \brief Gets the parameter types of the function type.
		 * \return The parameter types of the function type.
		 */
		[[nodiscard]]
		const std::vector<TypeInfo>& getParameterTypes() const;

		/**
		 * \brief Gets the result type of the function type.
		 * \return The result type of the function type.
		 */
		[[nodiscard]]
		TypeInfo getResultType() const;

		/**
		 * \brief Check whether the function type is pure or not.
		 * \return Whether the function type is pure or not.
		 */
		[[nodiscard]]
		bool isPure() const;

		/**
		 * \brief Check whether the function type is free or not.
		 * \return Whether the function type is free or not.
		 */
		[[nodiscard]]
		bool isFree() const;

		CONSTRUCT_WITH_CHECKED_CAST(FunctionInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Function)
	};

	class VariantInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(VariantInfo, TypeInfo)
		static VariantInfo create(const std::vector<TypeInfo>& variant_types);

		[[nodiscard]]
		const std::vector<TypeInfo>& getUnderlyingTypes() const;

		[[nodiscard]]
		TypeInfo getMember(usize index) const;

		CONSTRUCT_WITH_CHECKED_CAST(VariantInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Variant)
	};

	class ClassInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(ClassInfo, TypeInfo)

		/**
		 * Gets the SymID of the class type.
		 * @return The SymID of the class type.
		 */
		[[nodiscard]]
		compiler::helios::SymID getSymbol() const;

		/**
		 * Gets the type of the base class.
		 * @param ctx The Query Context necessary to refer to the definition of the class.
		 * @return The type of the base class.
		 */
		[[nodiscard]]
		base::Optional<ClassInfo> getBaseClassType(query::Context& ctx) const;

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
		std::vector<ClassInfo> getImplementedInterfaceTypes(query::Context& ctx) const;
		// @TODO: change return type to InterfaceInfo when interface type is created.

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
		TypeInfo getMemberType(compiler::helios::SymID sym, query::Context& ctx) const;

		CONSTRUCT_WITH_CHECKED_CAST(ClassInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Class)
	};

	/***********************\
	|  MISCELLANEOUS TYPES  |
	\***********************/

	class NamespaceInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(NamespaceInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(NamespaceInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Namespace)
	};

	class ModuleInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(ModuleInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(ModuleInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Module)
	};

	// The Meta type is the type which hold values being types.
	// One could write, for example, `type myInt64 = i64;`.
	// Might be useful for generic interfaces and compile time code execution.
	class MetaInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(MetaInfo, TypeInfo)

		CONSTRUCT_WITH_CHECKED_CAST(MetaInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(Meta)
	};
}
