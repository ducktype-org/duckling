#include "type_info.hpp"

#include <sstream>
#include <concepts>

#include "internal/type_info_impl.hpp"
#include "templates.hpp"
#include "type_desc.hpp"
#include <base/exceptions.hpp>

/**
 * \brief Explicitly instantiate the `checkDynamicCast` template.
 * \param ClassName The class name from the `TypeInfo` hierarchy.
 */
#define INSTANTIATE_CHECKED_CAST(ClassName) \
	template ClassName::CPimpl checkDynamicCast<ClassName>(TypeInfo::CPimpl);

namespace ts {
	[[nodiscard]]
	Kind TypeInfo::getKind() const {
		return pimpl->getKind();
	}

	[[nodiscard]]
	usize TypeInfo::getSize() const {
		return pimpl->getSize();
	}

	[[nodiscard]]
	const std::string& TypeInfo::show() const {
		return pimpl->show();
	}

	[[nodiscard]]
	bool TypeInfo::isInfoImplicitlyCoercible(const TypeInfo to) const {
		return pimpl->isInfoImplicitlyCoercible(to);
	}

	UnitInfo UnitInfo::create() {
		// TODO: do we want to store unit_info on the vector as well?
		static auto unit_impl = internal::UnitInfoImpl{};
		static auto unit_info = UnitInfo{ &unit_impl };

		return unit_info;
	}

	VoidInfo VoidInfo::create() {
		// TODO: do we want to store void_info on the vector as well?
		static auto void_impl = internal::VoidInfoImpl{};
		static auto void_info = VoidInfo{ &void_impl };

		return void_info;
	}

	ByteInfo ByteInfo::create() {
		static auto byteImpl = Impl{};
		return ByteInfo{ &byteImpl };
	}

	BoolInfo BoolInfo::create() {
		static auto boolImpl = Impl{};
		return BoolInfo{ &boolImpl };
	}

	CharInfo CharInfo::create() {
		static auto charImpl = Impl{};
		return CharInfo{ &charImpl };
	}

	IntegralInfo IntegralInfo::create(usize size, bool signedness) {
		static std::map<std::pair<usize, bool>, Impl> ints = {
			{ { 8, true }, Impl{ 8, true } },     { { 8, false }, Impl{ 8, false } },
			{ { 16, true }, Impl{ 16, true } },   { { 16, false }, Impl{ 16, false } },
			{ { 32, true }, Impl{ 32, true } },   { { 32, false }, Impl{ 32, false } },
			{ { 64, true }, Impl{ 64, true } },   { { 64, false }, Impl{ 64, false } },
			{ { 128, true }, Impl{ 128, true } }, { { 128, false }, Impl{ 128, false } },
		};

		RIFT_ASSERT(ints.find({ size, signedness }) != ints.end(), "Incorrect simple int size");

		return IntegralInfo{ &ints.at({ size, signedness }) };
	}

	FloatInfo FloatInfo::create(usize size) {
		static std::map<usize, Impl> floats = {
			{ 16, Impl{ 16 } },  // For certain GPU applications
			{ 32, Impl{ 32 } },  // Standard float
			{ 64, Impl{ 64 } },  // Double precision
			{ 80, Impl{ 80 } },  // Long double, covers sum of ranges of int64 and uint64 precisely
			{ 128, Impl{ 128 } },  // Quad precision
		};

		RIFT_ASSERT(floats.find(size) != floats.end(), "Incorrect simple float size");

		return FloatInfo{ &floats.at(size) };
	}

	RawPointerInfo RawPointerInfo::create() {
		static auto raw_pointer_impl = Impl{};
		static auto raw_pointer      = RawPointerInfo{ &raw_pointer_impl };

		return raw_pointer;
	}

	PointerInfo PointerInfo::create(const TypeDesc<>& underlying_type) {
		static base::Map<TypeDesc<>, PointerInfo> pointers;

		if (!pointers.contains(underlying_type)) {
			auto pointer = base::make_unique<Impl>(underlying_type);

			pointers.put(underlying_type, PointerInfo{ pointer.get() });

			internal::pushType(pointer.release());
		}

		return pointers[underlying_type];
	}

	TypeDesc<> PointerInfo::getUnderlying() const { return ((CPimpl) pimpl)->getUnderlying(); }

	FunctionInfo FunctionInfo::create(
		const std::vector<TypeDesc<>>& parameter_types, TypeDesc<> result_type, i32 flags
	) {
		static base::Map<std::tuple<std::vector<TypeDesc<>>, TypeDesc<>, int>, FunctionInfo>
			function_types;
		if (function_types.contains({ parameter_types, result_type, flags }))
			return function_types[{ parameter_types, result_type, flags }];
		auto ptr = base::make_unique<Impl>(parameter_types, result_type, flags);

		function_types.put({ parameter_types, result_type, flags }, FunctionInfo(ptr.get()));

		internal::pushType(std::move(ptr));

		return function_types[{ parameter_types, result_type, flags }];
	}

	base::FlagType FunctionInfo::getFlags() const { return ((CPimpl) pimpl)->getFlags(); }

	std::vector<TypeDesc<>> FunctionInfo::getParameterTypeList() const {
		return ((CPimpl) pimpl)->getParameterList();
	}

	TypeDesc<> FunctionInfo::getResultType() const { return ((CPimpl) pimpl)->getResult(); }

	EnumInfo EnumInfo::create(const IntegralInfo& type_info) {
		auto enum_impl_p = base::make_unique<Impl>(type_info);
		auto enum_impl   = EnumInfo{ enum_impl_p.get() };

		internal::pushType(std::move(enum_impl_p));

		return enum_impl;
	}

	IntegralInfo EnumInfo::getBaseType() const { return ((CPimpl) pimpl)->getBaseType(); }

	FlagInfo FlagInfo::create(const IntegralInfo& type_info) {
		auto flag_impl_p = base::make_unique<Impl>(type_info);
		auto flag_impl   = FlagInfo{ flag_impl_p.get() };

		internal::pushType(std::move(flag_impl_p));

		return flag_impl;
	}

	IntegralInfo FlagInfo::getBaseType() const { return ((CPimpl) pimpl)->getBaseType(); }

	OptionalInfo OptionalInfo::create(const TypeDesc<>& underlying_type) {
		static base::Map<TypeDesc<>, OptionalInfo> optionals;

		if (!optionals.contains(underlying_type)) {
			auto optional = base::make_unique<Impl>(underlying_type);

			optionals.put(underlying_type, OptionalInfo{ optional.get() });

			internal::pushType(optional.release());
		}

		return optionals[underlying_type];
	}

	TypeDesc<> OptionalInfo::getUnderlying() const { return ((CPimpl) pimpl)->getUnderlying(); }

	// TODO: tupleInfo and variantInfo look nearly identical
	TupleInfo TupleInfo::create(const std::vector<TypeDesc<>>& tuple_types) {
		static base::Map<std::vector<TypeDesc<>>, TupleInfo> tuples;
		if (tuples.contains(tuple_types)) return tuples[tuple_types];

		auto ptr = base::make_unique<Impl>(tuple_types);

		tuples.put(tuple_types, TupleInfo{ ptr.get() });

		internal::pushType(std::move(ptr));

		return tuples[tuple_types];
	}

	const std::vector<TypeDesc<>>& TupleInfo::getUnderlyingTypes() const {
		return ((CPimpl) pimpl)->getUnderlyingTypes();
	}

	std::pair<TypeDesc<>, usize> TupleInfo::getMember(usize index) const {
		return ((CPimpl) pimpl)->getMember(index);
	}

	VariantInfo VariantInfo::create(const std::vector<TypeDesc<>>& variant_types) {
		static base::Map<std::vector<TypeDesc<>>, VariantInfo> variants;
		if (variants.contains(variant_types)) return variants[variant_types];

		auto ptr = base::make_unique<Impl>(variant_types);

		variants.put(variant_types, VariantInfo{ ptr.get() });

		internal::pushType(std::move(ptr));

		return variants[variant_types];
	}

	const std::vector<TypeDesc<>>& VariantInfo::getUnderlyingTypes() const {
		return ((CPimpl) pimpl)->getUnderlyingTypes();
	}

	TypeDesc<> VariantInfo::getMember(usize index) const {
		return ((CPimpl) pimpl)->getMember(index);
	}

	TypeTemplateInfo TypeTemplateInfo::create(std::vector<TypeDesc<>>& parameter_list) {
		static base::Map<std::vector<TypeDesc<>>, TypeTemplateInfo> type_templates;
		if (type_templates.contains(parameter_list)) return type_templates[parameter_list];
		auto ptr = base::make_unique<Impl>(parameter_list);

		type_templates.put(parameter_list, TypeTemplateInfo{ ptr.get() });

		internal::pushType(std::move(ptr));

		return type_templates[parameter_list];
	}

	std::vector<TypeDesc<>> TemplateInfo::getParameterList() const {
		return ((CPimpl) pimpl)->getParameterList();
	}

	NamespaceInfo NamespaceInfo::create() {
		static auto namespace_impl = Impl{};
		static auto namespace_info = NamespaceInfo{ &namespace_impl };

		return namespace_info;
	}

	CodeBlockInfo CodeBlockInfo::create() {
		static auto code_block_impl = Impl{};
		static auto code_block_info = CodeBlockInfo{ &code_block_impl };

		return code_block_info;
	}

	ModuleInfo ModuleInfo::create() {
		static auto module_impl = Impl{};
		static auto module_info = ModuleInfo{ &module_impl };

		return module_info;
	}

	MetaInfo MetaInfo::create() {
		static auto meta = Impl{};
		return MetaInfo{ &meta };
	}

	template<std::derived_from<TypeInfo> TYPE_INFO>
	typename TYPE_INFO::CPimpl checkDynamicCast(TypeInfo::CPimpl p) {
		auto result = dynamic_cast<typename TYPE_INFO::CPimpl>(p);
		if (result == nullptr) {
			std::stringstream ss;
			const Kind originalKind = p->getKind();  // Optimization-dependent mystery warning here.
			ss << "Type cast between TypeInfo kinds failed. A cast from "
			   << kindToString(originalKind) << " to " << kindToString(TYPE_INFO::getDefaultKind())
			   << " was attempted.";
			throw base::LogicError{ ss.str() };
		}
		return result;
	}

	INSTANTIATE_CHECKED_CAST(TypeInfo)
	INSTANTIATE_CHECKED_CAST(UnitInfo)
	INSTANTIATE_CHECKED_CAST(VoidInfo)
	INSTANTIATE_CHECKED_CAST(ByteInfo)
	INSTANTIATE_CHECKED_CAST(BoolInfo)
	INSTANTIATE_CHECKED_CAST(CharInfo)
	INSTANTIATE_CHECKED_CAST(IntegralInfo)
	INSTANTIATE_CHECKED_CAST(FloatInfo)
	INSTANTIATE_CHECKED_CAST(RawPointerInfo)
	INSTANTIATE_CHECKED_CAST(PointerInfo)
	INSTANTIATE_CHECKED_CAST(FunctionInfo)
	INSTANTIATE_CHECKED_CAST(EnumInfo)
	INSTANTIATE_CHECKED_CAST(FlagInfo)
	INSTANTIATE_CHECKED_CAST(OptionalInfo)
	INSTANTIATE_CHECKED_CAST(TupleInfo)
	INSTANTIATE_CHECKED_CAST(VariantInfo)
	INSTANTIATE_CHECKED_CAST(TemplateInfo)
	INSTANTIATE_CHECKED_CAST(TypeTemplateInfo)
	INSTANTIATE_CHECKED_CAST(NamespaceInfo)
	INSTANTIATE_CHECKED_CAST(CodeBlockInfo)
	INSTANTIATE_CHECKED_CAST(ModuleInfo)
	INSTANTIATE_CHECKED_CAST(ClassInfo)
	INSTANTIATE_CHECKED_CAST(VTableInfo)
	INSTANTIATE_CHECKED_CAST(MetaInfo)
}
