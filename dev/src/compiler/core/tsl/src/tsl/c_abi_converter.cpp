#include <helios/tsh/kind.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <tsl/c_abi_converter.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>

#include <variant>

namespace compiler::tsl {

	namespace {
		namespace ats = abi::type_system;

		CAbiConversionResult fail(std::string reason) {
			return CAbiConversionResult{ .abi_type = {}, .reason = std::move(reason) };
		}

		CAbiConversionResult ok(ats::AbiType type) {
			return CAbiConversionResult{ .abi_type = std::move(type), .reason = {} };
		}

		/** @brief Forward declaration so helpers below can recurse on field types. */
		CAbiConversionResult convert(compiler::tsh::SymbolType<> field_type, query::Context& ctx);

		CAbiConversionResult convertIntegral(compiler::tsh::IntegralAbstractType integral) {
			const usize width = usize(integral.getSize());
			if (width != 8 && width != 16 && width != 32 && width != 64)
				return fail(
					base::strConcat("unsupported integer width ", std::to_string(width), " bits")
				);
			const bool is_signed = integral.getSignedness()
			                    == compiler::tsh::IntegralAbstractType::Signedness::Signed;
			return ok(ats::intType(u8(width), is_signed));
		}

		CAbiConversionResult convertStaticArray(
			compiler::tsh::StaticArrayAbstractType array, query::Context& ctx
		) {
			const usize count = array.getSize();
			if (count == 0) return fail("zero-length array");

			CAbiConversionResult element = convert(array.getElementType(), ctx);
			if (!element.abi_type.has_value())
				return fail(base::strConcat("array element rejected: ", element.reason));

			return ok(ats::arrayType(std::move(*element.abi_type), count));
		}

		CAbiConversionResult convertClass(
			compiler::tsh::ClassAbstractType class_type, query::Context& ctx
		) {
			const compiler::helios::SymbolABI abi = class_type.getABI(ctx);
			if (!std::holds_alternative<compiler::helios::CAbi>(abi))
				return fail("nested non-extern(\"C\") class");

			const CRef<compiler::tsh::TypeInterface> iface = class_type.getInterface(ctx);

			std::vector<ats::Field> abi_fields;
			usize                   field_index = 0;
			for (const auto& element: iface->getElements()) {
				if (!element.isField()) continue;
				CAbiConversionResult sub = convert(element.getType(ctx), ctx);
				if (!sub.abi_type.has_value())
					return fail(base::strConcat(
						"nested class field #", std::to_string(field_index), " rejected: ", sub.reason
					));
				abi_fields.push_back(
					ats::field(base::Optional<std::string>{}, std::move(*sub.abi_type))
				);
				++field_index;
			}

			if (abi_fields.empty()) return fail("class has no fields (zero size in C ABI)");

			return ok(ats::structType(std::move(abi_fields)));
		}

		CAbiConversionResult convert(compiler::tsh::SymbolType<> field_type, query::Context& ctx) {
			using compiler::tsh::Kind;
			using compiler::tsh::ReferenceKind;

			switch (field_type.getRefKind()) {
			case ReferenceKind::Box:
				return fail("owning reference (Box)");
			case ReferenceKind::Ref:
				return fail("borrowed reference (Ref)");
			case ReferenceKind::Direct:
				break;
			}

			const compiler::tsh::AbstractType abstract = field_type.getType();
			switch (abstract.getKind()) {
			case Kind::Integral:
				return convertIntegral(compiler::tsh::IntegralAbstractType(abstract));
			case Kind::Byte:
				return ok(ats::intType(u8(8), false));
			case Kind::RawPointer:
				return ok(ats::pointerType());
			case Kind::StaticArray:
				return convertStaticArray(compiler::tsh::StaticArrayAbstractType(abstract), ctx);
			case Kind::Class:
				return convertClass(compiler::tsh::ClassAbstractType(abstract), ctx);
			case Kind::Pointer:
				return fail("typed pointer is not C-compatible; use `cptr T`");
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
	}

	CAbiConversionResult tryConvertToCAbiType(
		compiler::tsh::SymbolType<> field_type, query::Context& ctx
	) {
		return convert(field_type, ctx);
	}

}
