#include <query_framework/query_impl.hpp>
#include "../internal/type_info_impl.hpp"
#include "types.hpp"
#include "implicit_coercibility.hpp"

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
		class ErrorBadIntegralSize final: public dia::Error {
			usize requested_size;

		protected:
			[[nodiscard]]
			printer::PrinterContent toPrinterContentBrief() const override {
				std::stringstream ss;
				ss << "Invalid size of integral type: " << requested_size;
				return ss.str();
			}

			[[nodiscard]]
			printer::PrinterContent toPrinterContentDetailed() const override {
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

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryIntegralType, "QueryIntegralType");

	struct ImplementationOf_QueryFloatType:
		  query::QueryImplementation<QueryFloatType, FloatInfo::Pimpl> {
		class ErrorBadFloatSize final: public dia::Error {
			usize requested_size;

		protected:
			[[nodiscard]]
			printer::PrinterContent toPrinterContentBrief() const override {
				std::stringstream ss;
				ss << "Invalid size of float type: " << requested_size;
				return ss.str();
			}

			[[nodiscard]]
			printer::PrinterContent toPrinterContentDetailed() const override {
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

	struct ImplementationOf_QueryFunctionType:
		  query::QueryImplementation<QueryFunctionType, FunctionInfo::Pimpl> {
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

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryFunctionType, "QueryFunctionType");

	struct ImplementationOf_QueryNamespaceType:
		  query::QueryImplementation<QueryNamespaceType, NamespaceInfo::Pimpl> {
		static auto provide(Context&, QKey) -> PResult {
			static auto namespace_impl = internal::NamespaceInfoImpl{};
			return &namespace_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryNamespaceType, "QueryNamespaceType");

	struct ImplementationOf_QueryModuleType:
		  query::QueryImplementation<QueryModuleType, ModuleInfo::Pimpl> {
		static auto provide(Context&, QKey) -> PResult {
			static auto module_impl = internal::ModuleInfoImpl{};
			return &module_impl;
		}

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, const PResult p_res, query::ACD) -> QResult {
			return QResult{ p_res };
		}
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_QueryModuleType, "QueryModuleType");

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
