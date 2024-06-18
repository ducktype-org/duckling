#include <query_framework/query_impl.hpp>
#include "../internal/type_info_impl.hpp"
#include "types.hpp"

namespace ts {
	struct IMPLEMENT_QUERY(QueryUnitType, UnitInfo::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto unit_impl = internal::UnitInfoImpl{};
			return &unit_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryUnitType)

	struct IMPLEMENT_QUERY(QueryVoidType, VoidInfo::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto void_impl = internal::VoidInfoImpl{};
			return &void_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryVoidType)

	struct IMPLEMENT_QUERY(QueryByteType, ByteInfo::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto byte_impl = internal::ByteInfoImpl{};
			return &byte_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryByteType)

	struct IMPLEMENT_QUERY(QueryBoolType, BoolInfo::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto bool_impl = internal::BoolInfoImpl{};
			return &bool_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryBoolType)

	struct IMPLEMENT_QUERY(QueryCharType, CharInfo::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto char_impl = internal::CharInfoImpl{};
			return &char_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCharType)

	struct IMPLEMENT_QUERY(QueryIntegralType, IntegralInfo::Pimpl) {
		class ErrorBadIntegralSize final: public dia::Error {
			usize requested_size;

		protected:
			[[nodiscard]]
			std::string toStringBrief() const override {
				std::stringstream ss;
				ss << "Invalid size of integral type: " << requested_size;
				return ss.str();
			}

			[[nodiscard]]
			std::string toStringDetailed() const override {
				std::stringstream ss;
				ss << "Invalid size of integral type: " << requested_size << "\n"
				   << "The only allowed sizes are 8, 16, 32, 64, and 128.";
				return ss.str();
			}

		public:
			[[nodiscard]]
			Domain getDomain() const override {
				return Domain::TypeCheck;
			}

			explicit ErrorBadIntegralSize(
				const dia::SourcePosition& source_position, const usize requested_size
			):
				  Error(source_position),
				  requested_size(requested_size) {}
		};

		static auto provide(Context& context, const QKey key) -> PResult {
			using Impl = IntegralInfo::Impl;

			static std::map<std::pair<usize, bool>, Impl> cache = {
				{ { 8, true }, Impl{ 8, true } },     { { 8, false }, Impl{ 8, false } },
				{ { 16, true }, Impl{ 16, true } },   { { 16, false }, Impl{ 16, false } },
				{ { 32, true }, Impl{ 32, true } },   { { 32, false }, Impl{ 32, false } },
				{ { 64, true }, Impl{ 64, true } },   { { 64, false }, Impl{ 64, false } },
				{ { 128, true }, Impl{ 128, true } }, { { 128, false }, Impl{ 128, false } },
			};

			const auto [size, signedness] = key;

			if (!cache.contains({ size, signedness })) {
				// @FIXME: provide proper SourcePosition.
				context.log(base::make_unique<ErrorBadIntegralSize>(
					dia::SourcePosition::fakePosition(), size
				));
				// @TODO: maybe change to some ErrorType, instead of a "best guess".
				return &cache.at({ 128, signedness });
			}

			return &cache.at({ size, signedness });
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryIntegralType)

	struct IMPLEMENT_QUERY(QueryFloatType, FloatInfo::Pimpl) {
		class ErrorBadFloatSize final: public dia::Error {
			usize requested_size;

		protected:
			[[nodiscard]]
			std::string toStringBrief() const override {
				std::stringstream ss;
				ss << "Invalid size of float type: " << requested_size;
				return ss.str();
			}

			[[nodiscard]]
			std::string toStringDetailed() const override {
				std::stringstream ss;
				ss << "Invalid size of float type: " << requested_size << "\n"
				   << "The only allowed sizes are 16, 32, 64, 80, and 128.";
				return ss.str();
			}

		public:
			[[nodiscard]]
			Domain getDomain() const override {
				return Domain::TypeCheck;
			}

			explicit ErrorBadFloatSize(
				const dia::SourcePosition& source_position, const usize requested_size
			):
				  Error(source_position),
				  requested_size(requested_size) {}
		};

		static auto provide(Context& context, const QKey size) -> PResult {
			using Impl = FloatInfo::Impl;

			static std::map<usize, Impl> cache = {
				{ 16, Impl{ 16 } },    // For certain GPU applications
				{ 32, Impl{ 32 } },    // Standard float
				{ 64, Impl{ 64 } },    // Double precision
				{ 80, Impl{ 80 } },    // Long double, covers int64 and uint64 precisely
				{ 128, Impl{ 128 } },  // Quad precision
			};

			if (!cache.contains(size)) {
				// @FIXME: provide proper SourcePosition.
				context.log(
					base::make_unique<ErrorBadFloatSize>(dia::SourcePosition::fakePosition(), size)
				);
				// @TODO: maybe change to some ErrorType, instead of a "best guess".
				return &cache.at(128);
			}

			return &cache.at(size);
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryFloatType)

	struct IMPLEMENT_QUERY(QueryRawPointerType, RawPointerInfo::Pimpl) {
		static auto provide(Context&, QKey key) -> PResult {
			static auto rawPointer_impl = std::array<internal::RawPointerInfoImpl, 2>{
				internal::RawPointerInfoImpl{ false }, internal::RawPointerInfoImpl{ true }
			};
			return &rawPointer_impl.at(key);
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryRawPointerType)

	struct IMPLEMENT_QUERY(QueryPointerType, PointerInfo::Pimpl) {
		static inline base::Map<QKey, query::CacheEntry<PointerInfo>> cache;

		static auto provide(Context&, const QKey key) -> PResult {
			const auto pointer_pimpl = new internal::PointerInfoImpl{ key };
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

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPointerType)

	struct IMPLEMENT_QUERY(QueryTupleType, TupleInfo::Pimpl) {
		static inline base::Map<QKey, query::CacheEntry<TupleInfo>> cache;

		static auto provide(Context&, const QKey& key) -> PResult {
			const auto tuple_pimpl = new internal::TupleInfoImpl{ key.components };
			pushType(base::unique_ptr(tuple_pimpl));
			return tuple_pimpl;
		}

		static auto store(const QKey& key, const PResult p_res, const query::ACD acd) -> QResult {
			const auto q_res = QResult{ p_res };
			cache.emplace(key, query::CacheEntry<QResult>{ q_res, acd });
			return q_res;
		}

		static auto load(const QKey& key) -> LoadResult {
			if (const auto cache_iter = cache.find(key); cache_iter != cache.end())
				return base::Optional{ cache_iter->second };
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTupleType)

	struct IMPLEMENT_QUERY(QueryFunctionType, FunctionInfo::Pimpl) {
		static inline base::Map<QKey, query::CacheEntry<FunctionInfo>> cache;

		static auto provide(Context&, const QKey& key) -> PResult {
			const auto [params, result, pure, free] = key;
			const auto function_pimpl
				= new internal::FunctionInfoImpl{ params, result, pure, free };
			pushType(base::unique_ptr(function_pimpl));
			return function_pimpl;
		}

		static auto store(const QKey& key, const PResult p_res, const query::ACD acd) -> QResult {
			const auto q_res = QResult{ p_res };
			cache.emplace(key, query::CacheEntry<QResult>{ q_res, acd });
			return q_res;
		}

		static auto load(const QKey& key) -> LoadResult {
			if (const auto cache_iter = cache.find(key); cache_iter != cache.end())
				return base::Optional{ cache_iter->second };
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryFunctionType)

	struct IMPLEMENT_QUERY(QueryClassType, ClassInfo::Pimpl) {
		static inline base::Map<QKey, query::CacheEntry<ClassInfo>> cache;

		static auto provide(Context&, QKey key) -> PResult {
			const auto class_pimpl = new internal::ClassInfoImpl{ key };
			pushType(base::unique_ptr(class_pimpl));
			return class_pimpl;
		}

		static auto store(QKey key, const PResult p_res, query::ACD acd) -> QResult {
			const auto q_res = QResult{ p_res };
			cache.emplace(key, query::CacheEntry<QResult>{ q_res, acd });
			return q_res;
		}

		static auto load(QKey key) -> LoadResult {
			if (const auto cache_iter = cache.find(key); cache_iter != cache.end())
				return base::Optional{ cache_iter->second };
			return {};
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassType)

	struct IMPLEMENT_QUERY(QueryNamespaceType, NamespaceInfo::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto namespace_impl = internal::NamespaceInfoImpl{};
			return &namespace_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryNamespaceType)

	struct IMPLEMENT_QUERY(QueryModuleType, ModuleInfo::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto module_impl = internal::ModuleInfoImpl{};
			return &module_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryModuleType)

	struct IMPLEMENT_QUERY(QueryMetaType, MetaInfo::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto meta_impl = internal::MetaInfoImpl{};
			return &meta_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMetaType)
}
