#include <abi/layout/compute_c_layout.hpp>
#include <abi/type_system/type.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/kind.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <tsl/c_abi_converter.hpp>
#include <tsl/c_abi_target.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace compiler::tsl {

	namespace ats = abi::type_system;

	namespace {

		CAbiConversionResult fail(std::string reason) { return std::unexpected(std::move(reason)); }

		CAbiConversionResult ok(ats::AbiType type) {
			return CAbiConversionResult{ std::move(type) };
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

		CAbiConversionResult convertFloat(tsh::FloatAbstractType flt) {
			const usize width = usize(flt.getSize());
			// A float is C-compatible iff the target lists its width: the IEEE
			// formats everywhere, the x87 80-bit extended only on x87 targets.
			if (!compilerTargetABI().data_layout.float_layouts.atMaybe(u8(width)).has_value())
				return fail(base::strConcat(
					"floating-point width ",
					std::to_string(width),
					" bits is not representable on the target ABI"
				));
			return ok(ats::floatType(u8(width)));
		}

		CAbiConversionResult convertStaticArray(
			tsh::StaticArrayAbstractType array, query::Context& ctx
		) {
			if (array.getSize() == 0) return fail("zero-length array");

			const auto& element_conv
				= ctx.query<QueryCAbiTypeOf>(array.getElementType())->valueOrThrow();
			if (!element_conv.has_value())
				return fail(base::strConcat("array element rejected: ", element_conv.error()));


			auto array_abi_type = ats::arrayType(ats::cloneAbiType(*element_conv), array.getSize());

			auto size_align = sizeAlignOf(compilerTargetABI(), array_abi_type);

			return ok(ats::AbiType{ ats::OpaqueType{
				.size      = size_align.size,
				.alignment = size_align.alignment,
			} });
		}

		CAbiConversionResult convertClass(tsh::ClassAbstractType class_type, query::Context& ctx) {
			const helios::SymbolABI abi = class_type.getABI(ctx);
			variant_match(abi) {
				variant_case_novalue(helios::DefaultAbi) {
					return fail("nested non-extern(\"C\") class");
				}
				variant_case_novalue(helios::CAbi) {
					// Convert the fields to their C-ABI types, compute the class's
					// C layout from them and return it as an opaque blob.
					std::vector<ats::AbiTypePtr> fields;
					for (const auto& element: class_type.getInterface(ctx)->getElements()) {
						if (!element.isField()) continue;
						const auto& conversion
							= ctx.query<QueryCAbiTypeOf>(element.getType(ctx))->valueOrThrow();
						if (!conversion.has_value())
							return fail(base::strConcat(
								"field `",
								helios::name(element.getSymbol()),
								"` rejected: ",
								conversion.error()
							));
						fields.push_back(ats::makeAbiType(ats::cloneAbiType(*conversion)));
					}
					if (fields.empty()) return fail("class with no fields has zero size in C ABI");

					auto size_align
						= sizeAlignOf(compilerTargetABI(), ats::structType(std::move(fields)));

					return ok(ats::AbiType{ ats::OpaqueType{
						.size      = size_align.size,
						.alignment = size_align.alignment,
					} });
				}
			}
			CORE_PANIC("unknown symbol ABI kind");
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
				return convertFloat(tsh::FloatAbstractType(abstract));
			case Kind::Bool:
				return ok(ats::boolType());
			case Kind::Char:
				return ok(ats::charType());
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
			case tsh::Kind::Slice:
				return fail("`slice` is not C-compatible");
			}
			CORE_UNREACHABLE();
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCAbiTypeOf)

}
