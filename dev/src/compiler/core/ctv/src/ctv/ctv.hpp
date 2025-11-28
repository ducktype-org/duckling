#pragma once

#include <ctv/numeric_value.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>

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

			/**
			 * @brief Checks whether the tuple can be interpreted as a type.
			 * This is true iff all elements of the tuple can be interpreted as types.
			 * @return True if the tuple can be interpreted as a type, false otherwise.
			 * @TODO: #1618 Remove
			 */
			[[nodiscard]]
			bool canBeType() const {
				for (const auto& element: elements)
					if (!element.canBeType()) return false;
				return true;
			}

		private:
			std::vector<CompileTimeValue> elements;
		};

	private:
		using Storage = std::variant<bool, NumericValue, UnitCTV, TupleCTV, tsh::SymbolType<>>;
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
		 * @brief Retrieves the value of type from the CTV.
		 * @note Possibly converts tuple and unit values to types.
		 * @param ctx The query context for lifting unit value to unit type.
		 * @return A type value or an empty optional if the CTV didn't store a type.
		 */
		[[nodiscard]]
		base::Optional<tsh::SymbolType<>> getType(query::Context& ctx) const;

		/**
		 * @brief Checks whether the CTV can be interpreted as a type.
		 *
		 * For example:
		 * CTVs that are symbol types can always be interpreted as types.
		 * Boolean and integral CTVs can never be interpreted as types.
		 * Tuples can be interpreted as types iff all their elements can.
		 *
		 * @return True if the CTV can be interpreted as a type, false otherwise.
		 */
		[[nodiscard]]
		bool canBeType() const;

		/**
		 * @brief Returns the compiler::tsh::SymbolType based on the value stored in the CTV.
		 * @param ctx The query context for lifting unit value to unit type.
		 * @return The type of the value stored in the CTV.
		 */
		[[nodiscard]] tsh::SymbolType<> getTypeOfStoredValue(query::Context& ctx) const;
	};
}
