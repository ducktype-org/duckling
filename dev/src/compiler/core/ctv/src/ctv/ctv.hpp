#pragma once

#include <ctv/numeric_value.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <string_id/string_id.hpp>

#include <string>

namespace compiler::ctv {
	using numeric_value::NumericValue;

	/**
	 * @brief Represents a value known at compile time.
	 */
	class CompileTimeValue {
	public:
		struct UnitCTV {};

		struct TupleCTV {
			explicit TupleCTV(std::vector<CompileTimeValue> elements):
				  elements(std::move(elements)) {
				// We require at least two elements to distinguish it from:
				// - UnitCTV, which has zero elements, and
				// - a single CTV that happens to appear in parentheses.
				CORE_ASSERT(this->elements.size() >= 2, "TupleCTV must have at least two elements");
			}

			/**
			 * @brief Get the elements of the tuple CTV.
			 * @return The vector of the tuple CTV's elements.
			 */
			[[nodiscard]]
			const std::vector<CompileTimeValue>& getElements() const {
				return elements;
			}

		private:
			std::vector<CompileTimeValue> elements;
		};

	private:
		using Storage
			= std::variant<bool, NumericValue, char, base::StrID, UnitCTV, TupleCTV, tsh::SymbolType<>>;
		Storage value;

	public:
		CompileTimeValue();

		/**
		 * @brief Template constructor of CTV for all types which exist in the Storage variant.
		 */
		template<typename T>
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>) CompileTimeValue(T val): value(val) {}

		/**
		 * @brief Returns a constant reference to the CTVs internal value storage.
		 * @return A constant reference to the CTV value storage.
		 */
		[[nodiscard]]
		const Storage& getStorage() const;

		/**
		 * @brief Transforms the value stored in the CTV to a string representation. Used for debug
		 * purposes.
		 * @return A string representation of the value stored in the CTV.
		 */
		[[nodiscard]]
		std::string toString() const;

		/**
		 * @brief Retrieves the value of the given type from the CTV.
		 * @return A stored value or an empty optional if the CTV didn't store the requested type.
		 */
		template<typename T>
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>)
		[[nodiscard]] base::Optional<T> get() const {
			variant_match(value) {
				variant_case(T, val) { return val; }
			}
			return {};
		}

		/**
		 * @brief Checks whether a CTV stores a value of a given type.
		 * @return True, if the value of a given type is stored in the CTV, false otherwise.
		 */
		template<typename T>
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>) [[nodiscard]] bool has() const {
			return std::holds_alternative<T>(value);
		}

		/**
		 * @brief Returns the compiler::tsh::SymbolType based on the value stored in the CTV.
		 * @param ctx The query context for lifting unit value to unit type.
		 * @return The type of the value stored in the CTV.
		 */
		[[nodiscard]] tsh::SymbolType<> getTypeOfStoredValue(query::Context& ctx) const;
	};
}
