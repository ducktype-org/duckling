#include <helios/tsh/kind.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <tsl/c_abi_converter.hpp>
#include <tsl/queries.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/types/bits_and_bytes.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <variant>

namespace compiler::tsl {

	namespace ats = abi::type_system;

	namespace {

		CAbiConversionResult fail(std::string reason) {
			return CAbiConversionResult{ .abi_type = {}, .reason = std::move(reason) };
		}

		CAbiConversionResult ok(ats::AbiType type) {
			return CAbiConversionResult{ .abi_type = std::move(type), .reason = {} };
		}

		CAbiConversionResult convertIntegral(tsh::IntegralAbstractType integral) {
			const usize width = usize(integral.getSize());
			if (width != 8 && width != 16 && width != 32 && width != 64)
				return fail(
					base::strConcat("unsupported integer width ", std::to_string(width), " bits")
				);
			const bool is_signed
				= integral.getSignedness() == tsh::IntegralAbstractType::Signedness::Signed;
			return ok(ats::intType(u8(width), is_signed));
		}

		CAbiConversionResult convertStaticArray(
			tsh::StaticArrayAbstractType array, query::Context& ctx
		) {
			if (array.getSize() == 0) return fail("zero-length array");

			// Validate the element type; a non-C-compatible element rejects the array.
			const auto& element_conv
				= ctx.query<QueryCAbiTypeOf>(array.getElementType())->valueOrThrow();
			if (!element_conv.abi_type.has_value())
				return fail(base::strConcat("array element rejected: ", element_conv.reason));

			// Represent the whole array as an opaque blob carrying its computed layout.
			const auto& layout = ctx.query<QueryAbstractTypeLayout>(array)->valueOrThrow();
			return ok(
				ats::opaqueType(base::bits2bytesRoundUp(layout.getSize()), layout.getAlignment())
			);
		}

		CAbiConversionResult convertClass(tsh::ClassAbstractType class_type, query::Context& ctx) {
			const helios::SymbolABI abi = class_type.getABI(ctx);
			if (!std::holds_alternative<helios::CAbi>(abi))
				return fail("nested non-extern(\"C\") class");

			const auto& layout = ctx.query<QueryAbstractTypeLayout>(class_type)->valueOrThrow();
			return ok(
				ats::opaqueType(base::bits2bytesRoundUp(layout.getSize()), layout.getAlignment())
			);
		}
	}

	struct IMPLEMENT_QUERY(QueryCAbiTypeOf, query::QResult<CAbiConversionResult>) {
		static auto provide(Context& ctx, const QKey& key) -> PResult {
			using tsh::Kind;
			using tsh::ReferenceKind;

			switch (key.getRefKind()) {
			case ReferenceKind::Box:
				return fail("owning reference (Box)");
			case ReferenceKind::Ref:
				return fail("borrowed reference (Ref)");
			case ReferenceKind::Direct:
				break;
			}

			const tsh::AbstractType abstract = key.getType();
			switch (abstract.getKind()) {
			case Kind::Integral:
				return convertIntegral(tsh::IntegralAbstractType(abstract));
			case Kind::Byte:
				return ok(ats::intType(u8(8), false));
			case Kind::RawPointer:
				return fail("raw pointer is not C-compatible; use `cptr T`");
			case Kind::CPointer:
				return ok(ats::pointerType());
			case Kind::StaticArray:
				return convertStaticArray(tsh::StaticArrayAbstractType(abstract), ctx);
			case Kind::Class:
				return convertClass(tsh::ClassAbstractType(abstract), ctx);
			case Kind::Pointer:
				return fail("typed pointer is not C-compatible; use `cptr T`");
			case Kind::ManyPointer:
				return fail("many-pointer is not C-compatible; use `cptr T`");
			case Kind::Tuple:
				return fail("tuples are not C-compatible");
			case Kind::Variant:
				return fail("variants are not C-compatible");
			case Kind::Float:
				return fail("floating-point types are not supported");
			case Kind::Bool:
				return fail("`bool` is not C-compatible");
			case Kind::Char:
				return fail("`char` is not C-compatible");
			case Kind::String:
				return fail("`string` is not C-compatible");
			case Kind::DynamicArray:
				return fail("dynamic arrays are not C-compatible");
			case Kind::Function:
				return fail("function types are not C-compatible");
			case Kind::Unit:
				return fail("type has zero size in C ABI");
			case Kind::Void:
				return fail("`void` cannot appear as a field type");
			case Kind::Meta:
				return fail("meta-type cannot appear as a field type");
			case Kind::Namespace:
			case Kind::Module:
			case Kind::Import:
			case Kind::TypeTemplate:
				return fail("non-runtime type cannot appear as a field type");
			case Kind::COUNT:
				CORE_UNREACHABLE();
			}
			CORE_UNREACHABLE();
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCAbiTypeOf)

}
