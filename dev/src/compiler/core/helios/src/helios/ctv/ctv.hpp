#pragma once

#include <typesystem/higher/symbol_type.hpp>

#include <base/variant.hpp>

#include <variant>

namespace compiler::helios {
	// TODOP: What about the VmValue.
	// using CompileTimeValue = std::variant<i64, bool, tsh::SymbolType<>>;

	/**
	 * @brief Represents a value known at compile time.
	 */
	class CompileTimeValue {
	private:
		using Storage = std::variant<i64, bool, tsh::SymbolType<>>;
		Storage value;

	public:
		CompileTimeValue() = default;

		CompileTimeValue(i64 val): value(val) {}

		CompileTimeValue(bool val): value(val) {}

		CompileTimeValue(tsh::SymbolType<> val): value(val) {}

		[[nodiscard]] base::Optional<i64> asI64() const {
			variant_match(value) {
				variant_case(i64, val) { return val; }
				// TODOP: Case for VmValue?
			}
			return {};
		}

		[[nodiscard]] base::Optional<bool> asBool() const {
			variant_match(value) {
				variant_case(bool, val) { return val; }
				// TODOP: Case for VmValue?
			}
			return {};
		}

		[[nodiscard]] base::Optional<tsh::SymbolType<>> asType() const {
			variant_match(value) {
				variant_case(tsh::SymbolType<>, val) { return val; }
				// TODOP: Case for VmValue?
			}
			return {};
		}

		[[nodiscard]] const Storage& getStorage() const { return value; }
	};


}
