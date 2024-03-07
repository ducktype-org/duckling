/**
 * \file type_info.hpp
 * \brief Interfaces of the simpler kinds of types.
 *
 * The interface is not aware of the internal implementation hierarchy in any way other than its
 * existence and name.
 */

#pragma once
#include "type_info.hpp"

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
		class FunctionInfoImpl;
		class VariantInfoImpl;
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

		/**
		 * \brief Create an instance of the Unit type.
		 * \return The Unit type.
		 */
		static UnitInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(UnitInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(UnitInfo)
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

		/**
		 * \brief Create an instance of the Void type.
		 * \return The Void type.
		 */
		static VoidInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(VoidInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(VoidInfo)
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

		/**
		 * \brief Create an instance of the Byte type.
		 * \return The Byte type.
		 */
		static ByteInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(ByteInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(ByteInfo)
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

		/**
		 * \brief Create an instance of the Bool type.
		 * \return The Bool type.
		 */
		static BoolInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(BoolInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(BoolInfo)
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

		/**
		 * \brief Create an instance of the Char type.
		 * \return The Char type.
		 */
		static CharInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(CharInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(CharInfo)
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

		/**
		 * \brief Create an instance of an Integral type.
		 * \param size The size of the Integral type.
		 * \param signedness The signedness of the Integral type.
		 * \return The Integral Type.
		 */
		static IntegralInfo create(usize size, bool signedness = true);

		CONSTRUCT_WITH_CHECKED_CAST(IntegralInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(IntegralInfo)
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

		/**
		 * \brief Create an instance of a Float type.
		 * \param size The size of the Float type.
		 * \return The Float Type.
		 */
		static FloatInfo create(usize size);

		CONSTRUCT_WITH_CHECKED_CAST(FloatInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(FloatInfo)
	};

	/*******************\
	|   POINTER TYPES   |
	\*******************/

	/**
	 * \brief The Raw Pointer type.
	 *
	 * A value of this type is simply a memory address.
	 */
	class RawPointerInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(RawPointerInfo, TypeInfo)
		/**
		 * \brief Create the (single) instance of a RawPointer type.
		 * \return The RawPointer type.
		 */
		static RawPointerInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(RawPointerInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(RawPointerInfo)
	};

	/**
	 * \brief The (typed) Pointer types.
	 *
	 * A value of this type is simply a memory address.
	 * However, it is statically known what the type of the pointee is.
	 */
	class PointerInfo final: public RawPointerInfo {
	public:
		SETUP_TYPE_WITH_BASE(PointerInfo, RawPointerInfo)

		/**
		 * \brief Create an instance of a Pointer type.
		 * \param underlying_type The underlying type of the Pointer type.
		 * \return The Pointer type.
		 */
		static PointerInfo create(const TypeDesc<>& underlying_type);

		/**
		 * \brief Gets the underlying type of the Pointer type.
		 * \return The underlying type.
		 */
		[[nodiscard]]
		TypeDesc<> getUnderlyingType() const;

		CONSTRUCT_WITH_CHECKED_CAST(PointerInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(PointerInfo)
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
		 * \param leaking Whether the reference can leak its value. By default, references are
		 * non-leaking. \param nullable Whether the reference can be empty. By default, references
		 * cannot be empty. \param unique Whether the reference is unique. By default, references
		 * can be copied and shared. \return The created reference type.
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
		CONSTRUCT_FROM_IMPLEMENTATION(ReferenceInfo)
	};

	/*******************\
	|  COMPOSITE TYPES  |
	\*******************/

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

		static FunctionInfo create(
			const std::vector<TypeDesc<>>& parameter_types,
			TypeDesc<>                     result_type,
			bool                           pure = false,
			bool                           free = false
		);

		/**
		 * \brief Gets the parameter types of the function type.
		 * \return The parameter types of the function type.
		 */
		[[nodiscard]]
		std::vector<TypeDesc<>> getParameterTypes() const;

		/**
		 * \brief Gets the result type of the function type.
		 * \return The result type of the function type.
		 */
		[[nodiscard]]
		TypeDesc<> getResultType() const;

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
		CONSTRUCT_FROM_IMPLEMENTATION(FunctionInfo)
	};

	class VariantInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(VariantInfo, TypeInfo)
		static VariantInfo create(const std::vector<TypeDesc<>>& variant_types);

		const std::vector<TypeDesc<>>& getUnderlyingTypes() const;

		TypeDesc<> getMember(usize index) const;

		CONSTRUCT_WITH_CHECKED_CAST(VariantInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(VariantInfo)
	};

	class NamespaceInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(NamespaceInfo, TypeInfo)
		static NamespaceInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(NamespaceInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(NamespaceInfo)
	};

	class ModuleInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(ModuleInfo, TypeInfo)
		static ModuleInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(ModuleInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(ModuleInfo)
	};

	class MetaInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(MetaInfo, TypeInfo)
		static MetaInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(MetaInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(MetaInfo)
	};
}
