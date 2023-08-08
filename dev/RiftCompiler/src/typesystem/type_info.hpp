/**
 * @file type_desc.hpp
 * @brief TypeInfo implementation
 */

#pragma once

#include "kind.hpp"
#include <cstddef>
#include <string>
#include <vector>

#define CHECKED_CAST(ClassName)                                       \
	template<std::derived_from<TypeInfo> T>                           \
	ClassName(const T& other): Base((const BPimpl)other.getPimpl()) { \
		checkDynamicCast<CPimpl>(other.getPimpl());                   \
	}


#define CONSTRUCT_FROM_IMPLEMENTATION(ClassName) \
	explicit ClassName(const Pimpl pimpl): Base((BPimpl)pimpl) {}


#define SETUP_TYPE(ClassName, BaseClass)     \
	using Impl = internal::ClassName##Impl;  \
	using BImpl = internal::BaseClass##Impl; \
	using Base = BaseClass;                  \
	using Pimpl = Impl*;                     \
	using CPimpl = const Impl*;              \
	using BPimpl = BImpl*;                   \
	using CBPimpl = const BImpl*;


namespace ts {
	// this is const, and not constexpr, because it might be defined during runtime in the future
	const usize META_SIZE = 64;
	const usize POINTER_SIZE = 64;
	const usize BYTE_SIZE = 8;
	const usize BOOL_SIZE = BYTE_SIZE;
	const usize CHAR_SIZE = BYTE_SIZE;
	namespace internal {
		class TypeInfoImpl;
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

	template<typename T>
	T checkDynamicCast(const internal::TypeInfoImpl*);

	class TypeInfo {
		using Pimpl = internal::TypeInfoImpl*;

	public:
		[[nodiscard]] Kind getKind() const;
		[[nodiscard]] usize getSize() const;
		TypeInfo() = delete;

		template<std::derived_from<TypeInfo> T>
		explicit TypeInfo(const T& other): pimpl(other.pimpl) {
			checkDynamicCast<const internal::TypeInfoImpl*>(other.pimpl);
		}

		[[nodiscard]] auto operator<=>(const TypeInfo& other) const = default;

		[[nodiscard]] const internal::TypeInfoImpl* getPimpl() const { return pimpl; }

		[[nodiscard]] bool isInfoImplicitlyCoercible(const TypeInfo to) const;

		[[nodiscard]] const std::string& show() const;

	protected:
		explicit TypeInfo(const internal::TypeInfoImpl* pimpl): pimpl(pimpl) {}

		// This is almost-const, but we need assignment operator on TypeInfo.
		const internal::TypeInfoImpl* pimpl;
	};
}
