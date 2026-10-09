// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "ctv.hpp"

#include <ctv/numeric_value.hpp>
#include <helios/tsh/queries/types.hpp>

#include <base/str/str_utils.hpp>

#include <hashing/add_to_hash.hpp>
#include <hashing/hashing_algorithms.hpp>
#include <query_framework/context/context.hpp>
#include <string_id/string_id.hpp>

#include <vm/core/vmvalue/ivmvalue.hpp>

#include <iomanip>
#include <span>
#include <sstream>
#include <string>

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
			variant_case(tsh::SymbolType<>, val) { return val.toString(); }
			variant_case(VMValue, vm_value) {
				std::stringstream ss;
				ss << "type: `" << vm_value.type.toString() << "`\n";
				vm_value.val->dprint(ss);
				return ss.str();
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
