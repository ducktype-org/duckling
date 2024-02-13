/**
 * @file type_info.hpp
 * @brief Interfaces of the simpler kinds of types.
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
		class FunctionInfoImpl;
		class EnumInfoImpl;
		class FlagInfoImpl;
		class OptionalInfoImpl;
		class TupleInfoImpl;
		class VariantInfoImpl;
		class TemplateInfoImpl;
		class TypeTemplateInfoImpl;
		class NamespaceInfoImpl;
		class CodeBlockInfoImpl;
		class ModuleInfoImpl;
		class MetaInfoImpl;
	}

	class UnitInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(UnitInfo, TypeInfo)
		static UnitInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(UnitInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(UnitInfo)
	};

	class VoidInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(VoidInfo, TypeInfo)
		static VoidInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(VoidInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(VoidInfo)
	};

	class ByteInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(ByteInfo, TypeInfo)
		static ByteInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(ByteInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(ByteInfo)
	};

	class BoolInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(BoolInfo, TypeInfo)
		static BoolInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(BoolInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(BoolInfo)
	};

	class CharInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(CharInfo, TypeInfo)
		static CharInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(CharInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(CharInfo)
	};

	class IntegralInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(IntegralInfo, TypeInfo)
		static IntegralInfo create(usize size, bool signedness = true);

		CONSTRUCT_WITH_CHECKED_CAST(IntegralInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(IntegralInfo)
	};

	class FloatInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(FloatInfo, TypeInfo)
		static FloatInfo create(usize size);

		CONSTRUCT_WITH_CHECKED_CAST(FloatInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(FloatInfo)
	};

	class RawPointerInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(RawPointerInfo, TypeInfo)
		static RawPointerInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(RawPointerInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(RawPointerInfo)
	};

	class PointerInfo: public RawPointerInfo {
	public:
		SETUP_TYPE_WITH_BASE(PointerInfo, RawPointerInfo)
		static PointerInfo create(const TypeDesc<>& underlying_type);

		TypeDesc<> getUnderlying() const;

		CONSTRUCT_WITH_CHECKED_CAST(PointerInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(PointerInfo)
	};

	class FunctionInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(FunctionInfo, TypeInfo)
		static FunctionInfo create(
			const std::vector<TypeDesc<>>& parameter_types, TypeDesc<> result_type, i32 flags = 0
		);

		[[nodiscard]]
		base::FlagType getFlags() const;

		[[nodiscard]]
		std::vector<TypeDesc<>> getParameterTypeList() const;

		[[nodiscard]]
		TypeDesc<> getResultType() const;

		CONSTRUCT_WITH_CHECKED_CAST(FunctionInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(FunctionInfo)
	};

	class EnumInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(EnumInfo, TypeInfo)
		static EnumInfo create(const IntegralInfo& base_type);
		IntegralInfo    getBaseType() const;

		CONSTRUCT_WITH_CHECKED_CAST(EnumInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(EnumInfo)
	};

	class FlagInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(FlagInfo, TypeInfo)
		static FlagInfo create(const IntegralInfo& base_type);
		IntegralInfo    getBaseType() const;

		CONSTRUCT_WITH_CHECKED_CAST(FlagInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(FlagInfo)
	};

	class OptionalInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(OptionalInfo, TypeInfo)
		static OptionalInfo create(const TypeDesc<>& underlying_type);

		TypeDesc<> getUnderlying() const;

		CONSTRUCT_WITH_CHECKED_CAST(OptionalInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(OptionalInfo)
	};

	class TupleInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(TupleInfo, TypeInfo)
		static TupleInfo create(const std::vector<TypeDesc<>>& variant_types);

		const std::vector<TypeDesc<>>& getUnderlyingTypes() const;

		std::pair<TypeDesc<>, usize> getMember(usize index) const;

		CONSTRUCT_WITH_CHECKED_CAST(TupleInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(TupleInfo)
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

	class CodeBlockInfo: public TypeInfo {
	public:
		SETUP_TYPE_WITH_BASE(CodeBlockInfo, TypeInfo)
		static CodeBlockInfo create();

		CONSTRUCT_WITH_CHECKED_CAST(CodeBlockInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(CodeBlockInfo)
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
