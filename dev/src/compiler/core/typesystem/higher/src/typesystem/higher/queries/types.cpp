#include "types.hpp"

#include <typesystem/higher/abstract_type_impl.hpp>

#include <query_framework/query_impl.hpp>

namespace tsh {
	struct IMPLEMENT_QUERY(QueryUnitType, UnitAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto unit_impl = UnitAbstractTypeImpl{};
			return &unit_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryUnitType)

	struct IMPLEMENT_QUERY(QueryVoidType, VoidAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto void_impl = VoidAbstractTypeImpl{};
			return &void_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryVoidType)

	struct IMPLEMENT_QUERY(QueryByteType, ByteAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto byte_impl = ByteAbstractTypeImpl{};
			return &byte_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryByteType)

	struct IMPLEMENT_QUERY(QueryBoolType, BoolAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto bool_impl = BoolAbstractTypeImpl{};
			return &bool_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryBoolType)

	struct IMPLEMENT_QUERY(QueryCharType, CharAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto char_impl = CharAbstractTypeImpl{};
			return &char_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCharType)

	struct IMPLEMENT_QUERY(QueryIntegralType, IntegralAbstractType::Pimpl) {
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
				ss << toStringBrief() << "\n"
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
			using Impl = IntegralAbstractType::Impl;
			using enum IntegralAbstractType::Signedness;

			static std::map<std::pair<usize, IntegralAbstractType::Signedness>, Impl> cache = {
				{ { 8, Signed }, Impl{ 8, Signed } },
				{ { 8, Unsigned }, Impl{ 8, Unsigned } },
				{ { 16, Signed }, Impl{ 16, Signed } },
				{ { 16, Unsigned }, Impl{ 16, Unsigned } },
				{ { 32, Signed }, Impl{ 32, Signed } },
				{ { 32, Unsigned }, Impl{ 32, Unsigned } },
				{ { 64, Signed }, Impl{ 64, Signed } },
				{ { 64, Unsigned }, Impl{ 64, Unsigned } },
				{ { 128, Signed }, Impl{ 128, Signed } },
				{ { 128, Unsigned }, Impl{ 128, Unsigned } },
			};

			const auto [size, signedness] = key;

			if (!cache.contains({ size, signedness })) {
				// @FIXME: provide proper SourcePosition.
				context.log(makeBox<ErrorBadIntegralSize>(dia::SourcePosition::fakePosition(), size)
				);
				// @TODO: maybe change to some ErrorType, instead of a "best guess".
				return &cache.at({ 128, signedness });
			}

			return &cache.at({ size, signedness });
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryIntegralType)

	struct IMPLEMENT_QUERY(QueryFloatType, FloatAbstractType::Pimpl) {
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
				ss << toStringBrief() << "\n"
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
			using Impl = FloatAbstractType::Impl;

			static std::map<usize, Impl> cache = {
				{ 16, Impl{ 16 } },    // For certain GPU applications
				{ 32, Impl{ 32 } },    // Standard float
				{ 64, Impl{ 64 } },    // Double precision
				{ 80, Impl{ 80 } },    // Long double, covers int64 and uint64 precisely
				{ 128, Impl{ 128 } },  // Quad precision
			};

			if (!cache.contains(size)) {
				// @FIXME: provide proper SourcePosition.
				context.log(makeBox<ErrorBadFloatSize>(dia::SourcePosition::fakePosition(), size));
				// @TODO: maybe change to some ErrorType, instead of a "best guess".
				return &cache.at(128);
			}

			return &cache.at(size);
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryFloatType)

	struct IMPLEMENT_QUERY(QueryRawPointerType, RawPointerAbstractType::Pimpl) {
		static auto provide(Context&, const QKey key) -> PResult {
			static auto raw_pointer_impl
				= std::array{ RawPointerAbstractTypeImpl{ Mutability::Immutable },
				              RawPointerAbstractTypeImpl{ Mutability::Mutable } };
			return &raw_pointer_impl.at(key);
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryRawPointerType)

	struct IMPLEMENT_QUERY(QueryPointerType, PointerAbstractType::Impl) {
		static auto provide(Context&, const QKey key) -> PResult {
			return PointerAbstractTypeImpl(key);
		}

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPointerType)

	struct IMPLEMENT_QUERY(QueryStringType, StringAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto string_impl = StringAbstractTypeImpl{};
			return &string_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryStringType)

	struct IMPLEMENT_QUERY(QueryDynamicArrayType, DynamicArrayAbstractType::Impl) {
		static auto provide(Context&, const QKey key) -> PResult { return { key }; }

		QUERY_AUTO_CACHE_CONSTRUCT_FROM_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDynamicArrayType)

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

	struct IMPLEMENT_QUERY(QueryNamespaceType, NamespaceAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto namespace_impl = NamespaceAbstractTypeImpl{};
			return &namespace_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryNamespaceType)

	struct IMPLEMENT_QUERY(QueryModuleType, ModuleAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto module_impl = ModuleAbstractTypeImpl{};
			return &module_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryModuleType)

	struct IMPLEMENT_QUERY(QueryMetaType, MetaAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto meta_impl = MetaAbstractTypeImpl{};
			return &meta_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMetaType)

	struct IMPLEMENT_QUERY(QueryImportType, ImportAbstractType::Pimpl) {
		static auto provide(Context&, QKey) -> PResult {
			static auto import_impl = ImportAbstractTypeImpl{};
			return &import_impl;
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryImportType)
}
