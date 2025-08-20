#include "ctv.hpp"

namespace compiler::helios {
	CompileTimeValue::CompileTimeValue() = default;

	const CompileTimeValue::Storage& CompileTimeValue::getStorage() const { return value; }

	std::string CompileTimeValue::toString() const {
		variant_match(value) {
			variant_case(i64, val) { return std::to_string(val); }
			variant_case(bool, val) { return val ? "true" : "false"; }
			variant_case(tsh::SymbolType<>, val) { return val.toString(); }
			variant_default {
				throw base::NotYetImplemented("Converting other CTV types to string");
			}
		}
		return "";
	}

	base::Optional<i64> CompileTimeValue::asI64() const {
		variant_match(value) {
			variant_case(i64, val) { return val; }
		}
		return {};
	}

	base::Optional<bool> CompileTimeValue::asBool() const {
		variant_match(value) {
			variant_case(bool, val) { return val; }
		}
		return {};
	}

	base::Optional<tsh::SymbolType<>> CompileTimeValue::asType() const {
		variant_match(value) {
			variant_case(tsh::SymbolType<>, val) { return val; }
		}
		return {};
	}
}
