/**
 * @file types.hpp
 * @brief Implementation of all non-class types
 */

#pragma once
#include "type_info.hpp"

namespace ts {
	class VoidInfo: public TypeInfo {
		SETUP_TYPE(VoidInfo, TypeInfo)
	public:
		static VoidInfo create();

		CHECKED_CAST(VoidInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(VoidInfo)
	};

	class ByteInfo: public TypeInfo {
		SETUP_TYPE(ByteInfo, TypeInfo)

	public:
		static ByteInfo create();

		CHECKED_CAST(ByteInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(ByteInfo)
	};

	class BoolInfo: public TypeInfo {
		SETUP_TYPE(BoolInfo, TypeInfo)

	public:
		static BoolInfo create();

		CHECKED_CAST(BoolInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(BoolInfo)
	};

	class CharInfo: public TypeInfo {
		SETUP_TYPE(CharInfo, TypeInfo)

	public:
		static CharInfo create();

		CHECKED_CAST(CharInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(CharInfo)
	};

	class IntegralInfo: public TypeInfo {
		SETUP_TYPE(IntegralInfo, TypeInfo)

	public:
		static IntegralInfo create(usize size, bool signedness = true);

		CHECKED_CAST(IntegralInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(IntegralInfo)
	};

	class FloatInfo: public TypeInfo {
		SETUP_TYPE(FloatInfo, TypeInfo)

	public:
		static FloatInfo create(usize size);

		CHECKED_CAST(FloatInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(FloatInfo)
	};

	class RawPointerInfo: public TypeInfo {
		SETUP_TYPE(RawPointerInfo, TypeInfo);

	public:
		static RawPointerInfo create();

		CHECKED_CAST(RawPointerInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(RawPointerInfo)
	};

	class PointerInfo: public RawPointerInfo {
		SETUP_TYPE(PointerInfo, RawPointerInfo)

	public:
		static PointerInfo create(const TypeDesc<>& underlying_type);

		TypeDesc<> getUnderlying() const;

		CHECKED_CAST(PointerInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(PointerInfo)
	};

	class FunctionInfo: public TypeInfo {
		SETUP_TYPE(FunctionInfo, TypeInfo)

	public:
		static FunctionInfo create(const std::vector<TypeDesc<>>& parameter_types,
		                           TypeDesc<> result_type, i32 flags = 0);

		[[nodiscard]] base::FlagType getFlags() const;

		[[nodiscard]] std::vector<TypeDesc<>> getParameterTypeList() const;

		[[nodiscard]] TypeDesc<> getResultType() const;

		CHECKED_CAST(FunctionInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(FunctionInfo)
	};

	class EnumInfo: public TypeInfo {
		SETUP_TYPE(EnumInfo, TypeInfo)

	public:
		static EnumInfo create(const IntegralInfo& base_type);
		IntegralInfo getBaseType() const;

		CHECKED_CAST(EnumInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(EnumInfo)
	};

	class FlagInfo: public TypeInfo {
		SETUP_TYPE(FlagInfo, TypeInfo)

	public:
		static FlagInfo create(const IntegralInfo& base_type);
		IntegralInfo getBaseType() const;

		CHECKED_CAST(FlagInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(FlagInfo)
	};

	class OptionalInfo: public TypeInfo {
		SETUP_TYPE(OptionalInfo, TypeInfo)

	public:
		static OptionalInfo create(const TypeDesc<>& underlying_type);

		TypeDesc<> getUnderlying() const;

		CHECKED_CAST(OptionalInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(OptionalInfo)
	};

	class TupleInfo: public TypeInfo {
		SETUP_TYPE(TupleInfo, TypeInfo)

	public:
		static TupleInfo create(const std::vector<TypeDesc<>>& variant_types);

		const std::vector<TypeDesc<>>& getUnderlyingTypes() const;

		std::pair<TypeDesc<>, usize> getMember(usize index) const;

		CHECKED_CAST(TupleInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(TupleInfo)
	};

	class VariantInfo: public TypeInfo {
		SETUP_TYPE(VariantInfo, TypeInfo)

	public:
		static VariantInfo create(const std::vector<TypeDesc<>>& variant_types);

		CHECKED_CAST(VariantInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(VariantInfo)
	};

	class NamespaceInfo: public TypeInfo {
		SETUP_TYPE(NamespaceInfo, TypeInfo)
	public:
		static NamespaceInfo create();

		CHECKED_CAST(NamespaceInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(NamespaceInfo)
	};

	class CodeBlockInfo: public TypeInfo {
		SETUP_TYPE(CodeBlockInfo, TypeInfo)
	public:
		static CodeBlockInfo create();

		CHECKED_CAST(CodeBlockInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(CodeBlockInfo)
	};

	class ModuleInfo: public TypeInfo {
		SETUP_TYPE(ModuleInfo, TypeInfo)

	public:
		static ModuleInfo create();

		CHECKED_CAST(ModuleInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(ModuleInfo)
	};

	class MetaInfo: public TypeInfo {
		SETUP_TYPE(MetaInfo, TypeInfo)

	public:
		static MetaInfo create();

		CHECKED_CAST(MetaInfo)
	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(MetaInfo)
	};
}