/**
 * @file abstract_type.hpp
 * @brief Interface of the AbstractType class.
 *
 * The interface is not aware of the internal implementation hierarchy
 * in any way other than its existence and name.
 */

#pragma once

#include "kind.hpp"

#include <base/ints.hpp>

#include <query_framework/context_fd.hpp>

#include <string>

/**
 * @brief Several type definitions for quick reference,
 * like Impl=internal::SomeAbstractType##Impl and Pimpl=Impl*.
 * @param SomeAbstractType The class name from the `AbstractType` hierarchy.
 */
#define SETUP_TYPE(SomeAbstractType)                 \
	using Impl   = internal::SomeAbstractType##Impl; \
	using Pimpl  = Impl*;                            \
	using CPimpl = const Impl*;

/**
 * @brief Several type definitions for quick reference,
 * like Impl=internal::SomeAbstractType##Impl and Pimpl=Impl*.
 * @param SomeAbstractType The class name from the AbstractType hierarchy.
 * @param BaseAbstractType The base class of SomeAbstractType. Since AbstractType itself does not
 * have a base class, this macro should not be used in the definition of AbstractType.
 */
#define SETUP_TYPE_WITH_BASE(SomeAbstractType, BaseAbstractType) \
	SETUP_TYPE(SomeAbstractType)                                 \
	using BImpl   = internal::BaseAbstractType##Impl;            \
	using Base    = BaseAbstractType;                            \
	using BPimpl  = BImpl*;                                      \
	using CBPimpl = const BImpl*;

/**
 * @brief Constructor from SomeAbstractType##Impl*.
 *
 * @param SomeAbstractType The class name from the AbstractType hierarchy.
 */
#define CONSTRUCT_FROM_IMPLEMENTATION(SomeAbstractType) \
	SomeAbstractType(const CPimpl pimpl): Base(reinterpret_cast<CBPimpl>(pimpl)) {}

/**
 * @brief Template constructor from the AbstractTypeImpl* hierarchy with a dynamic cast check.
 * @param SomeAbstractType The class name from the AbstractType hierarchy.
 */
#define CONSTRUCT_WITH_CHECKED_CAST(SomeAbstractType)             \
	template<std::derived_from<AbstractType> ABSTRACT_TYPE>       \
	explicit(false) SomeAbstractType(const ABSTRACT_TYPE& other): \
		  Base((const BPimpl) other.getPimpl()) {                 \
		checkDynamicCast<SomeAbstractType>(other.getPimpl());     \
	}

namespace tsh {
	namespace internal {
		class AbstractTypeImpl;
	}

	class AbstractType;
	class TypeInterface;

	template<std::derived_from<AbstractType> ABSTRACT_TYPE>
	typename ABSTRACT_TYPE::CPimpl checkDynamicCast(const internal::AbstractTypeImpl*);

	/**
	 * @brief The AbstractType class and its subclasses form a lightweight type interface hierarchy.
	 *
	 * An object from the AbstractType hierarchy, like IntegralInfo, FunctionInfo etc. hold a
	 * pointer to an implementation object (pImpl) from the internal::AbstractTypeImpl hierarchy.
	 *
	 * The AbstractType hierarchy is meant to be maximally lightweight. A type represented by a
	 * AbstractType object is uniquely identified by its underlying pImpl. This means that types
	 * represented by AbstractType objects are efficient to compare against each other. However,
	 * method calls are forwarded to the pImpl, which makes them cost extra in terms of jumps.
	 *
	 * Note, that the AbstractType hierarchy is visible to the rest of the compiler, while the
	 * internal::AbstractTypeImpl hierarchy is only visible in the .cpp file of the typesystem
	 * module.
	 */
	class AbstractType {
	public:
		SETUP_TYPE(AbstractType)

		/**
		 * @brief Get the Kind of the type described by this object.
		 * @return The Kind of the type described by this object.
		 */
		[[nodiscard]]
		Kind getKind() const;

		/**
		 * @brief Get the TypeInterface of the type described by this object.
		 * @param ctx The Query Context necessary to deduce interfaces.
		 * @return The TypeInterface of the type described by this object.
		 */
		[[nodiscard]]
		const TypeInterface& getInterface(query::Context& ctx) const;

		/**
		 * @brief Determines weather the type has a trivial destructor.
		 *
		 * It is needed to determine if createing a lifetime flag is needed during LIR lowering.
		 *
		 * @return true if the type has a trivial destructor, false otherwise.
		 */
		[[nodiscard]]
		bool hasNoOpDestructor() const;

		/**
		 * @brief The default constructor is deleted.
		 * This class must be instantiated only from meaningful pieces of data.
		 * See the other constructors.
		 */
		AbstractType() = delete;

		/**
		 * @brief Construct by upcasting.
		 *
		 * It is possible to cast up and down the AbstractType hierarchy. This means that
		 * an IntegralInfo object can be cast to a AbstractType object. After all, a description
		 * of an integral type is a description of just "a type".
		 *
		 * @tparam ABSTRACT_TYPE The type of the argument from the AbstractType hierarchy.
		 * @param other The object to be upcast from.
		 *
		 */
		template<std::derived_from<AbstractType> ABSTRACT_TYPE>
		explicit AbstractType(const ABSTRACT_TYPE& other): pimpl(other.pimpl) {
			checkDynamicCast<AbstractType>(other.pimpl);
		}

		/**
		 * @brief Compare with another AbstractType.
		 *
		 * The comparison is arbitrary and should only be used for
		 * indexing ordered data structures or comparing for equality.
		 *
		 * @param other The other AbstractType.
		 * @return The result of comparison, dependent on the value of the pImpl pointer.
		 */
		[[nodiscard]]
		auto operator<=>(const AbstractType& other) const
			= default;

		/**
		 * @brief Get pointer to the underlying AbstractTypeImpl object.
		 * @return Pointer to the underlying AbstractTypeImpl object.
		 */
		[[nodiscard]]
		const internal::AbstractTypeImpl* getPimpl() const {
			return pimpl;
		}

		/**
		 * @brief Determine whether it is legal to consider and implicit coercion
		 * from a value described by this ExpressionType to one described by target.
		 *
		 * An implicit coercion is when, for example, a boolean is expected, but
		 * and integer is given. A desirable (and common) behaviour may be to
		 * convert the integer value to true if and only if it is non-zero.
		 *
		 * Another context in which implicit coercions are desirable is when
		 * casting from subclass to superclass.
		 *
		 * This method does not determine how to perform a coercion.
		 * It only determines whether one should be considered.
		 * A coercion may thus be allowed but not implemented, or implemented
		 * but not allowed to be used implicitly by the compiler, so the user
		 * may define a coercion from class A to class B, but not want it
		 * to ever be used implicitly (in C++ that is achieved by annotating a
		 * single-argument constructor with the `explicit` keyword).
		 *
		 * This is typically determined by rules specific for the Kind of the source type.
		 *
		 * @param target The target of a hypothetical implicit coercion.
		 * @param context Context needed fo the query call.
		 * @return Whether the implicit coercion is allowed or not.
		 */
		[[nodiscard]]
		bool isImplicitlyCoercible(AbstractType target, query::Context& context) const;

		/**
		 * @brief Get the text representation of this type.
		 * @return The text representation of this type.
		 */
		[[nodiscard]]
		const std::string& toString() const;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;

	protected:
		/**
		 * @brief Construct from an object from the internal::AbstractTypeImpl hierarchy.
		 * @param pimpl A pointer to a type implementation object.
		 */
		AbstractType(const internal::AbstractTypeImpl* pimpl): pimpl(pimpl) {}

		friend class internal::AbstractTypeImpl;

		/**
		 * @brief The pointer to the (probably significantly heavier) object carrying
		 * the implementation which describes the types represented by this object.
		 */
		const internal::AbstractTypeImpl* pimpl;
	};
}
