#include "ctv.hpp"

#include <ctv/numeric_value.hpp>
#include <helios/tsh/queries/types.hpp>

#include <base/str/str_utils.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <query_framework/context/context.hpp>
#include <string_id/string_id.hpp>

#include <vm/core/vmvalue/ivmvalue.hpp>

#include <span>
#include <sstream>
#include <string>

namespace {
	namespace idv = vm::interpreted_data_variant;

	/**
	 * @brief Builds a short, human-readable representation of the VM value.
	 */
	std::string vmValueToString(const vm::IVMValue& value, const std::string& type_name) {
		auto data = value.readData();
		if (data.empty()) return base::strConcat("<", type_name, ">");

		variant_match(data.value()) {
			variant_case(idv::Primitive, primitive) { return std::to_string(primitive.value); }
			variant_case(idv::Data, data_value) {
				std::vector<std::string_view> names(data_value.fields.size());
				for (const auto& [name, index]: data_value.field_name_map)
					names.at(index) = name.strView();

				std::stringstream ss;
				ss << type_name << "{";
				for (usize i = 0; i < data_value.fields.size(); i++) {
					if (i > 0) ss << ", ";
					ss << names.at(i) << ": " << data_value.fields.at(i).value->str();
				}
				ss << "}";
				return ss.str();
			}
			variant_case(idv::Variant, variant) {
				return base::strConcat(
					type_name, "#", variant.type_tag, "(", variant.referenced->str(), ")"
				);
			}
			variant_case(idv::Table, table) {
				std::stringstream ss;
				ss << "[";
				for (usize i = 0; i < table.size; i++) {
					if (i > 0) ss << ", ";
					ss << table.get(i)->str();
				}
				ss << "]";
				return ss.str();
			}
			variant_case(idv::Pointer, pointer) {
				return pointer.referenced.has_value() ? "<pointer>" : "null";
			}
			variant_case_novalue(idv::Function) { return "<function>"; }
			variant_case_novalue(idv::Opaque) { return base::strConcat("<", type_name, ">"); }
		}
		CORE_UNREACHABLE();
	}
}

namespace compiler::ctv {
	const CompileTimeValue::Storage& CompileTimeValue::getStorage() const { return value; }

	base::Bit256 CompileTimeValue::queryUnstablePerfectHash() const {
		hashing::SHA256 hasher;
		hashing::addToHash(hasher, value.index());

		variant_match(value) {
			variant_case(bool, val) { hashing::addToHash(hasher, val); }
			variant_case(NumericValue, val) {
				hashing::addToHash(hasher, val.getStorage().index());
				VISIT(val.getStorage(), inner_value, hashing::addToHash(hasher, inner_value););
			}
			variant_case(char, c) { hashing::addToHash(hasher, c); }
			variant_case(CharSliceValue, val) { hashing::addToHash(hasher, val.value); }
			variant_case(StringClassValue, val) { hashing::addToHash(hasher, val.value); }
			variant_case_novalue(UnitCTV) {
				// nothing to add to hash
			}
			variant_case(TupleCTV, tuple) {
				throw base::NotYetImplemented(
					"Tuples are not supported yet in CTV::queryUnstablePerfectHash"
				);
			}
			variant_case(tsh::SymbolType<>, val) {
				hashing::addToHash(hasher, val.queryUnstablePerfectHash());
			}
			variant_case(VMValue, vm_value) {
				hashing::addToHash(hasher, vm_value.type.queryUnstablePerfectHash());
				hashing::addToHash(
					hasher,
					std::span<const byte>(
						vm_value.val->getBytes(),
						static_cast<usize>(vm_value.val->getDataSize().asInt())
					)
				);
			}
			variant_default { CORE_PANIC("Unhandled CTV type in CTV::queryUnstablePerfectHash"); }
		}

		return hasher.finalize();
	}

	std::string CompileTimeValue::toString() const {
		variant_match(value) {
			variant_case(bool, val) { return val ? "true" : "false"; }
			variant_case(NumericValue, val) { return val.toString(); }
			variant_case(char, c) { return "'" + base::escapeString(std::string{ c }) + "'"; }
			variant_case(CharSliceValue, val) {
				return "\"" + base::escapeString(val.value.str()) + "\"";
			}
			variant_case(StringClassValue, val) {
				return "\"" + base::escapeString(val.value.str()) + "\".toString()";
			}
			variant_case_novalue(UnitCTV) { return "()"; }
			variant_case(TupleCTV, tuple) {
				std::stringstream ss;
				ss << "(" << tuple.getElements().at(0).toString();
				for (usize i = 1; i < tuple.getElements().size(); i++)
					ss << ", " << tuple.getElements().at(i).toString();
				ss << ")";
				return ss.str();
			}
			variant_case(tsh::SymbolType<>, val) { return val.toString(); }
			variant_case(VMValue, vm_value) {
				return vmValueToString(*vm_value.val, vm_value.type.toString());
			}
			variant_default {
				throw base::NotYetImplemented("Converting other CTV types to string");
			}
		}
		return "";
	}

	tsh::SymbolType<> CompileTimeValue::getTypeOfStoredValue(query::Context& ctx) const {
		variant_match(value) {
			variant_case_novalue(bool) {
				return tsh::SymbolType<>{
					tsh::getBoolType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
			variant_case(NumericValue, numeric) { return numeric.getTypeOfStoredValue(ctx); }
			variant_case_novalue(char) {
				return tsh::SymbolType<>{
					tsh::getCharType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
			variant_case_novalue(CharSliceValue) {
				return tsh::SymbolType<>{
					tsh::getCharSliceType(ctx),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				};
			}
			variant_case_novalue(StringClassValue) {
				CORE_ASSERT(
					tsh::isStringTypePresent(ctx),
					"A String CTV cannot exist without the String type."
				);
				return tsh::SymbolType<>{
					tsh::getStringType(ctx),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Immutable,
				};
			}
			variant_case_novalue(UnitCTV) {
				return tsh::SymbolType<>{
					tsh::getUnitType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
			variant_case(TupleCTV, tuple) {
				std::vector<tsh::SymbolType<>> component_types;
				component_types.reserve(tuple.getElements().size());
				for (const auto& element: tuple.getElements())
					component_types.push_back(element.getTypeOfStoredValue(ctx));

				return tsh::SymbolType<>{
					ctx.query<tsh::QueryTupleType>({ std::move(component_types) }),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}

			variant_case(tsh::SymbolType<>, val) {
				return tsh::SymbolType<>{
					tsh::getMetaType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}
			variant_case(VMValue, vm_value) { return vm_value.type; }
		}
		CORE_UNREACHABLE();
	}
}
