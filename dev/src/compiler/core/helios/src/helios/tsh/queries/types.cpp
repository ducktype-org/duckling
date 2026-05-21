#include "types.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <helios_private/tsh/abstract_type_impl.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::tsh {
	UnitAbstractType getUnitType() {
		static auto unit_impl = UnitAbstractTypeImpl{};
		return UnitAbstractType{ &unit_impl };
	}

	VoidAbstractType getVoidType() {
		static auto void_impl = VoidAbstractTypeImpl{};
		return VoidAbstractType{ &void_impl };
	}

	ByteAbstractType getByteType() {
		static auto byte_impl = ByteAbstractTypeImpl{};
		return ByteAbstractType{ &byte_impl };
	}

	BoolAbstractType getBoolType() {
		static auto bool_impl = BoolAbstractTypeImpl{};
		return BoolAbstractType{ &bool_impl };
	}

	CharAbstractType getCharType() {
		static auto char_impl = CharAbstractTypeImpl{};
		return CharAbstractType{ &char_impl };
	}

	IntegralAbstractType getIntegralType(
		query::Context& ctx, u64 size, IntegralAbstractType::Signedness signedness
	) {
		using Impl = IntegralAbstractType::Impl;
		using enum IntegralAbstractType::Signedness;

		static const std::map<std::pair<usize, IntegralAbstractType::Signedness>, Impl> cache = {
			{ { 8, Signed }, Impl{ 8, Signed } },     { { 8, Unsigned }, Impl{ 8, Unsigned } },
			{ { 16, Signed }, Impl{ 16, Signed } },   { { 16, Unsigned }, Impl{ 16, Unsigned } },
			{ { 32, Signed }, Impl{ 32, Signed } },   { { 32, Unsigned }, Impl{ 32, Unsigned } },
			{ { 64, Signed }, Impl{ 64, Signed } },   { { 64, Unsigned }, Impl{ 64, Unsigned } },
			{ { 128, Signed }, Impl{ 128, Signed } }, { { 128, Unsigned }, Impl{ 128, Unsigned } },
		};

		if (!cache.contains({ size, signedness })) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				base::strConcat("Invalid size of integral type: ", size, "."),
				"The only allowed sizes are 8, 16, 32, 64 and 128."
			));
			// We might want to change this to query failed, instead of a "best guess".
			return IntegralAbstractType{ &cache.at({ 128, signedness }) };
		}

		return IntegralAbstractType{ &cache.at({ size, signedness }) };
	}

	FloatAbstractType getFloatType(query::Context& ctx, u64 size) {
		using Impl = FloatAbstractType::Impl;

		static const std::map<usize, Impl> cache = {
			{ 16, Impl{ 16 } },    // For certain GPU applications
			{ 32, Impl{ 32 } },    // Standard float
			{ 64, Impl{ 64 } },    // Double precision
			{ 80, Impl{ 80 } },    // Long double, covers int64 and uint64 precisely
			{ 128, Impl{ 128 } },  // Quad precision
		};

		if (!cache.contains(size)) {
			ctx.logInt(makeBox<dia_int::PlaceholderError>(
				base::strConcat("Invalid size of float type: ", size, "."),
				"The only allowed sizes are 16, 32, 64, 80, and 128."
			));
			// We might want to change this to query failed, instead of a "best guess".
			return FloatAbstractType{ &cache.at(128) };
		}

		return FloatAbstractType{ &cache.at(size) };
	}

	RawPointerAbstractType getRawPointerType(bool mutable_pointer) {
		static auto raw_pointer_impl
			= std::array{ RawPointerAbstractTypeImpl{ Mutability::Immutable },
			              RawPointerAbstractTypeImpl{ Mutability::Mutable } };
		return RawPointerAbstractType{ &raw_pointer_impl.at(mutable_pointer) };
	}

	StringAbstractType getStringType() {
		static auto string_impl = StringAbstractTypeImpl{};
		return StringAbstractType{ &string_impl };
	}

	NamespaceAbstractType getNamespaceType() {
		static auto namespace_impl = NamespaceAbstractTypeImpl{};
		return NamespaceAbstractType{ &namespace_impl };
	}

	MetaAbstractType getMetaType() {
		static auto meta_impl = MetaAbstractTypeImpl{};
		return MetaAbstractType{ &meta_impl };
	}

	ModuleAbstractType getModuleType() {
		static auto module_impl = ModuleAbstractTypeImpl{};
		return ModuleAbstractType{ &module_impl };
	}

	ImportAbstractType getImportType() {
		static auto import_impl = ImportAbstractTypeImpl{};
		return ImportAbstractType{ &import_impl };
	}

	struct IMPLEMENT_QUERY(QueryPointerType, PointerAbstractType::Impl) {
		static auto provide(Context&, const QKey key) -> PResult {
			return PointerAbstractTypeImpl(key);
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPointerType)

	struct IMPLEMENT_QUERY(QueryManyPointerType, ManyPointerAbstractType::Impl) {
		static auto provide(Context&, const QKey key) -> PResult {
			return ManyPointerAbstractTypeImpl(key);
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryManyPointerType)

	struct IMPLEMENT_QUERY(QueryCPointerType, CPointerAbstractType::Impl) {
		static auto provide(Context&, const QKey key) -> PResult {
			return CPointerAbstractTypeImpl(key);
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCPointerType)

	struct IMPLEMENT_QUERY(QueryDynamicArrayType, DynamicArrayAbstractType::Impl) {
		static auto provide(Context&, const QKey key) -> PResult { return { key }; }

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDynamicArrayType)

	struct IMPLEMENT_QUERY(QueryStaticArrayType, StaticArrayAbstractType::Impl) {
		static auto provide(Context&, const QKey key) -> PResult {
			return { key.element_type, key.size };
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryStaticArrayType)

	struct IMPLEMENT_QUERY(QueryTupleType, TupleAbstractType::Impl) {
		static auto provide(Context&, const QKey& key) -> PResult { return { key.components }; }

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTupleType)

	struct IMPLEMENT_QUERY(QueryVariantType, VariantAbstractType::Impl) {
		static auto provide(Context&, const QKey& key) -> PResult {
			return VariantAbstractTypeImpl(key.underlying_types);
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryVariantType)

	struct IMPLEMENT_QUERY(QueryFunctionType, FunctionAbstractType::Impl) {
		static auto provide(Context&, const QKey& key) -> PResult {
			const auto [params, result, pure, free] = key;
			return { params, result, pure, free };
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryFunctionType)

	struct IMPLEMENT_QUERY(QueryClassType, ClassAbstractType::Impl) {
		static auto provide(Context&, const QKey key) -> PResult {
			return ClassAbstractTypeImpl(key);
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassType)

	struct IMPLEMENT_QUERY(QueryTypeTemplateType, TypeTemplateAbstractType::Impl) {
		static auto provide(Context&, const QKey& key) -> PResult { return { key.source }; }

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeTemplateType)
}
