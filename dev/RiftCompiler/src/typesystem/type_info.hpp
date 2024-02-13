/**
 * @file type_desc.hpp
 * @brief TypeInfo implementation
 */

#pragma once

#include <base/ints.hpp>
#include "kind.hpp"
#include <string>
#include <vector>

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
 * \brief Constructor from ClassName##Impl*.
 * \param SomeTypeInfo The class name from the TypeInfo hierarchy.
 */
#define CONSTRUCT_FROM_IMPLEMENTATION(SomeTypeInfo) \
	explicit SomeTypeInfo(const Pimpl pimpl): Base((BPimpl) pimpl) {}

/**
 * \brief Several type definitions for quick reference, like Impl=internal::ClassName##Impl and
 * Pimpl=Impl*. \param SomeTypeInfo The class name from the `TypeInfo` hierarchy.
 */
#define SETUP_TYPE(SomeTypeInfo)                 \
	using Impl   = internal::SomeTypeInfo##Impl; \
	using Pimpl  = Impl*;                        \
	using CPimpl = const Impl*;

/**
 * \brief Several type definitions for quick reference, like Impl=internal::ClassName##Impl and
 * Pimpl=Impl*. \param SomeTypeInfo The class name from the `TypeInfo` hierarchy. \param
 * BaseTypeInfo The base class of `SomeTypeInfo`. Since `TypeInfo` itself does not have a base
 * class, this macro should not be used in the definition of `TypeInfo`.
 */
#define SETUP_TYPE_WITH_BASE(SomeTypeInfo, BaseTypeInfo) \
	SETUP_TYPE(SomeTypeInfo)                             \
	using BImpl   = internal::BaseTypeInfo##Impl;        \
	using Base    = BaseTypeInfo;                        \
	using BPimpl  = BImpl*;                              \
	using CBPimpl = const BImpl*;

namespace ts {
	// this is const, and not constexpr, because it might be defined during runtime in the future
	constexpr usize META_SIZE    = 64;
	constexpr usize POINTER_SIZE = 64;
	constexpr usize BYTE_SIZE    = 8;
	constexpr usize BOOL_SIZE    = BYTE_SIZE;
	constexpr usize CHAR_SIZE    = BYTE_SIZE;

	namespace internal {
		class TypeInfoImpl;
		class UnitInfoImpl;
		class VoidInfoImpl;
		class ByteInfoImpl;
		class BoolInfoImpl;
		class CharInfoImpl;
		class IntegralInfoImpl;
		class FloatInfoImpl;
		class RawPointerInfoImpl;
		class PointerInfoImpl;
		class FunctionInfoImpl;
		class EnumInfoImpl;
		class FlagInfoImpl;
		class OptionalInfoImpl;
		class TupleInfoImpl;
		class VariantInfoImpl;
		class ClassInfoImpl;
		class TemplateInfoImpl;
		class TypeTemplateInfoImpl;
		class NamespaceInfoImpl;
		class CodeBlockInfoImpl;
		class ModuleInfoImpl;
		class MetaInfoImpl;
		class VTableInfoImpl;
	}

	class TypeInfo;

	template<std::derived_from<TypeInfo> TYPE_INFO>
	typename TYPE_INFO::CPimpl checkDynamicCast(const internal::TypeInfoImpl*);

	class TypeInfo {
	public:
		SETUP_TYPE(TypeInfo)

		[[nodiscard]]
		Kind getKind() const;
		[[nodiscard]]
		usize getSize() const;

		[[nodiscard]]
		static Kind getDefaultKind() {
			return Kind::Any;
		}

		TypeInfo() = delete;

		template<std::derived_from<TypeInfo> TYPE_INFO>
		explicit TypeInfo(const TYPE_INFO& other): pimpl(other.pimpl) {
			checkDynamicCast<TypeInfo>(other.pimpl);
		}

		[[nodiscard]]
		auto operator<=>(const TypeInfo& other) const
			= default;

		[[nodiscard]]
		const internal::TypeInfoImpl* getPimpl() const {
			return pimpl;
		}

		[[nodiscard]]
		bool isInfoImplicitlyCoercible(const TypeInfo to) const;

		[[nodiscard]]
		const std::string& show() const;

	protected:
		explicit TypeInfo(const internal::TypeInfoImpl* pimpl): pimpl(pimpl) {}

		// This is almost-const, but we need assignment operator on TypeInfo.
		const internal::TypeInfoImpl* pimpl;
	};
}
