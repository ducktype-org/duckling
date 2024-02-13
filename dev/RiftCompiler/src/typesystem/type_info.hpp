/**
 * \file type_info.hpp
 * \brief Interface of the TypeInfo class.
 *
 * The interface is not aware of the internal implementation hierarchy
 * in any way other than its existence and name.
 */

#pragma once

#include <base/ints.hpp>
#include "kind.hpp"
#include <string>

/**
 * \brief Template constructor from the TypeInfoImpl* hierarchy with a dynamic cast check.
 * \param SomeTypeInfo The class name from the TypeInfo hierarchy.
 */
#define CONSTRUCT_WITH_CHECKED_CAST(SomeTypeInfo)                                 \
	template<std::derived_from<TypeInfo> TYPE_INFO>                               \
	SomeTypeInfo(const TYPE_INFO& other): Base((const BPimpl) other.getPimpl()) { \
		checkDynamicCast<SomeTypeInfo>(other.getPimpl());                         \
	}

/**
 * \brief Constructor from SomeTypeInfo##Impl*.
 * \param SomeTypeInfo The class name from the TypeInfo hierarchy.
 */
#define CONSTRUCT_FROM_IMPLEMENTATION(SomeTypeInfo) \
	explicit SomeTypeInfo(const Pimpl pimpl): Base((BPimpl) pimpl) {}

/**
 * \brief Several type definitions for quick reference,
 * like Impl=internal::SomeTypeInfo##Impl and Pimpl=Impl*.
 * \param SomeTypeInfo The class name from the `TypeInfo` hierarchy.
 */
#define SETUP_TYPE(SomeTypeInfo)                 \
	using Impl   = internal::SomeTypeInfo##Impl; \
	using Pimpl  = Impl*;                        \
	using CPimpl = const Impl*;

/**
 * \brief Several type definitions for quick reference,
 * like Impl=internal::SomeTypeInfo##Impl and Pimpl=Impl*.
 * \param SomeTypeInfo The class name from the TypeInfo hierarchy.
 * \param BaseTypeInfo The base class of SomeTypeInfo. Since TypeInfo itself does not have a
 * base class, this macro should not be used in the definition of TypeInfo.
 */
#define SETUP_TYPE_WITH_BASE(SomeTypeInfo, BaseTypeInfo) \
	SETUP_TYPE(SomeTypeInfo)                             \
	using BImpl   = internal::BaseTypeInfo##Impl;        \
	using Base    = BaseTypeInfo;                        \
	using BPimpl  = BImpl*;                              \
	using CBPimpl = const BImpl*;

namespace ts {
	// This may become const instead of constexpr because it might be defined during runtime.
	constexpr usize META_SIZE    = 64;
	constexpr usize POINTER_SIZE = 64;
	constexpr usize BYTE_SIZE    = 8;
	constexpr usize BOOL_SIZE    = BYTE_SIZE;
	constexpr usize CHAR_SIZE    = BYTE_SIZE;

	namespace internal {
		class TypeInfoImpl;
	}

	class TypeInfo;

	template<std::derived_from<TypeInfo> TYPE_INFO>
	typename TYPE_INFO::CPimpl checkDynamicCast(const internal::TypeInfoImpl*);

	/**
	 * \brief The TypeInfo class and its subclasses form a lightweight type interface hierarchy.
	 *
	 * An object from the TypeInfo hierarchy, like IntegralInfo, FunctionInfo etc. hold a
	 * pointer to an implementation object (pImpl) from the internal::TypeInfoImpl hierarchy.
	 *
	 * The TypeInfo hierarchy is meant to be maximally lightweight. A type represented by a TypeInfo
	 * object is uniquely identified by its underlying pImpl. This means that types represented by
	 * TypeInfo objects are efficient to compare against each other. However, method calls are
	 * forwarded to the pImpl, which makes them cost extra in terms of jumps.
	 *
	 * Note, that the TypeInfo hierarchy is visible to the rest of the compiler, while the
	 * internal::TypeInfoImpl hierarchy is only visible in the typesystem module.
	 */
	class TypeInfo {
	public:
		SETUP_TYPE(TypeInfo)

		/**
		 * \brief Get the Kind of the type described by this object.
		 * \return The Kind of the type described by this object.
		 */
		[[nodiscard]]
		Kind getKind() const;

		/**
		 * \brief Get the size of the type described by this object.
		 * \return The size in bits of the type described by this object.
		 */
		[[nodiscard]]
		usize getSize() const;

		/**
		 * \brief Get the Kind of types described by objects of this class.
		 * \return The Kind of types described by objects of this class.
		 */
		[[nodiscard]]
		static Kind getStaticKind() {
			return Kind::Any;
		}

		/**
		 * \brief The default constructor is deleted.
		 * This class must be instantiated only from meaningful pieces of data.
		 * See the other constructors.
		 */
		TypeInfo() = delete;

		/**
		 * \brief Construct by upcasting.
		 *
		 * It is possible to cast up and down the TypeInfo hierarchy. This means that
		 * an IntegralInfo object can be cast to a TypeInfo object. After all, a description
		 * of an integral type is a description of just "a type".
		 *
		 * \tparam TYPE_INFO The type of the argument from the TypeInfo hierarchy.
		 * \param other The object to be upcast from.
		 *
		 */
		template<std::derived_from<TypeInfo> TYPE_INFO>
		explicit TypeInfo(const TYPE_INFO& other): pimpl(other.pimpl) {
			checkDynamicCast<TypeInfo>(other.pimpl);
		}

		/**
		 * \brief Compare with another TypeInfo. The comparison is arbitrary and should
		 * only be used for indexing ordered data structures or comparing for equality.
		 * \param other The other TypeInfo.
		 * \return The result of comparison, dependent on the value of the pImpl pointer.
		 */
		[[nodiscard]]
		auto operator<=>(const TypeInfo& other) const
			= default;

		/**
		 * \brief Get pointer to the underlying TypeInfoImpl object.
		 * \return Pointer to the underlying TypeInfoImpl object.
		 */
		[[nodiscard]]
		const internal::TypeInfoImpl* getPimpl() const {
			return pimpl;
		}

		/**
		 * \brief Whether a value of the type described with this TypeInfo object
		 * is allowed to be implicitly coerced to a value of the target type.
		 *
		 * This is typically determined by rules specific for the Kind of the source type.
		 *
		 * \param target The target type.
		 * \return Whether implicit coercion from this target is allowed.
		 */
		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo target) const;

		/**
		 * \brief Get the text representation of this type.
		 * \return The text representation of this type.
		 */
		[[nodiscard]]
		const std::string& show() const;

	protected:
		/**
		 * \brief Construct from an object from the internal::TypeInfoImpl hierarchy.
		 * \param pimpl A pointer to a type implementation object.
		 */
		explicit TypeInfo(const internal::TypeInfoImpl* pimpl): pimpl(pimpl) {}

		// This is almost-const, but we need assignment operator on TypeInfo.
		const internal::TypeInfoImpl* pimpl;
	};
}
