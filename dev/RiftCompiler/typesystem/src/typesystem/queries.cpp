#include <query_framework/query_impl.hpp>
#include "internal/type_info_impl.hpp"
#include "queries.hpp"

namespace ts {
	struct ImplementationOf_QueryUnitType:
		  query::QueryImplementation<QueryUnitType, UnitInfo::Pimpl> {
		static auto provide(Context&, QKey) -> PResult {
			static auto unit_impl = internal::UnitInfoImpl{};
			return &unit_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryUnitType, "QueryUnitType");

	struct ImplementationOf_QueryVoidType:
		  query::QueryImplementation<QueryVoidType, VoidInfo::Pimpl> {
		static auto provide(Context&, QKey) -> PResult {
			static auto void_impl = internal::VoidInfoImpl{};
			return &void_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryVoidType, "QueryVoidType");

	struct ImplementationOf_QueryByteType:
		  query::QueryImplementation<QueryByteType, ByteInfo::Pimpl> {
		static auto provide(Context&, QKey) -> PResult {
			static auto byte_impl = internal::ByteInfoImpl{};
			return &byte_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryByteType, "QueryByteType");

	struct ImplementationOf_QueryBoolType:
		  query::QueryImplementation<QueryBoolType, BoolInfo::Pimpl> {
		static auto provide(Context&, QKey) -> PResult {
			static auto bool_impl = internal::BoolInfoImpl{};
			return &bool_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryBoolType, "QueryBoolType");

	struct ImplementationOf_QueryCharType:
		  query::QueryImplementation<QueryCharType, CharInfo::Pimpl> {
		static auto provide(Context&, QKey) -> PResult {
			static auto char_impl = internal::CharInfoImpl{};
			return &char_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryCharType, "QueryCharType");

	struct ImplementationOf_QueryIntegralType:
		  query::QueryImplementation<QueryIntegralType, IntegralInfo::Pimpl> {
		static auto provide(Context&, const QKey key) -> PResult {
			using Impl = IntegralInfo::Impl;

			static std::map<std::pair<usize, bool>, Impl> cache = {
				{ { 8, true }, Impl{ 8, true } },     { { 8, false }, Impl{ 8, false } },
				{ { 16, true }, Impl{ 16, true } },   { { 16, false }, Impl{ 16, false } },
				{ { 32, true }, Impl{ 32, true } },   { { 32, false }, Impl{ 32, false } },
				{ { 64, true }, Impl{ 64, true } },   { { 64, false }, Impl{ 64, false } },
				{ { 128, true }, Impl{ 128, true } }, { { 128, false }, Impl{ 128, false } },
			};

			const auto [size, signedness] = key;

			// @FIXME: This probably should be a properly logged compiler error,
			// waiting for diagnostics merge. Maybe we want to syntactically allow stuff like
			// i42 as a type, but reject it at the TS level?
			RIFT_ASSERT(cache.contains({ size, signedness }), "Incorrect simple int size");

			return &cache.at({ size, signedness });
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryIntegralType, "QueryIntegralType");

	struct ImplementationOf_QueryFloatType:
		  query::QueryImplementation<QueryFloatType, FloatInfo::Pimpl> {
		static auto provide(Context&, const QKey size) -> PResult {
			using Impl = FloatInfo::Impl;

			static std::map<usize, Impl> cache = {
				{ 16, Impl{ 16 } },    // For certain GPU applications
				{ 32, Impl{ 32 } },    // Standard float
				{ 64, Impl{ 64 } },    // Double precision
				{ 80, Impl{ 80 } },    // Long double, covers int64 and uint64 precisely
				{ 128, Impl{ 128 } },  // Quad precision
			};

			// @FIXME: This probably should be a properly logged compiler error,
			// waiting for diagnostics merge. Maybe we want to syntactically allow stuff like
			// f42 as a type, but reject it at the TS level?
			RIFT_ASSERT(cache.contains(size), "Incorrect simple float size");

			return &cache.at(size);
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryFloatType, "QueryFloatType");

	struct ImplementationOf_QueryRawPointerType:
		  query::QueryImplementation<QueryRawPointerType, RawPointerInfo::Pimpl> {
		static auto provide(Context&, QKey) -> PResult {
			static auto rawPointer_impl = internal::RawPointerInfoImpl{};
			return &rawPointer_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryRawPointerType, "QueryRawPointerType");

	struct ImplementationOf_QueryPointerType:
		  query::QueryImplementation<QueryPointerType, PointerInfo::Pimpl> {
		static inline base::Map<QKey, query::CacheEntry<PointerInfo>> cache;

		static auto provide(Context&, const QKey key) -> PResult {
			const auto [underlying_type, is_mutable] = key;
			const auto pointer_pimpl = new internal::PointerInfoImpl{ underlying_type, is_mutable };
			pushType(base::unique_ptr(pointer_pimpl));
			return pointer_pimpl;
		}

		static auto store(const QKey key, const PResult p_res, const query::ACD acd) -> QResult {
			const auto q_res = QResult{ p_res };
			cache.emplace(key, query::CacheEntry<QResult>{ q_res, acd });
			return q_res;
		}

		static auto load(const QKey key) -> LoadResult {
			if (const auto cache_iter = cache.find(key); cache_iter != cache.end())
				return base::Optional{ cache_iter->second };
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryPointerType, "QueryPointerType");

	struct ImplementationOf_QueryMetaType:
		  query::QueryImplementation<QueryMetaType, MetaInfo::Pimpl> {
		static auto provide(Context&, QKey) -> PResult {
			static auto meta_impl = internal::MetaInfoImpl{};
			return &meta_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryMetaType, "QueryMetaType");
}
