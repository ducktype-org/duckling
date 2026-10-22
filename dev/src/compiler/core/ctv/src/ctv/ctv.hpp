// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <ctv/numeric_value.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <string_id/string_id.hpp>

#include <vm/core/vmvalue/ivmvalue_fd.hpp>

#include <string>

namespace compiler::ctv {
	using numeric_value::NumericValue;

	/**
	 * @brief Represents a value known at compile time.
	 */
	class CompileTimeValue final {
	public:
		struct UnitCTV {};

		struct CharSliceValue {
			base::StrID value;
		};

		struct StringClassValue {
			base::StrID value;
		};

		/**
		 * @brief A value living in the compile-time DVM process, together with its compiler type.
		 * @note Valid only as long as the compile-time DVM process that owns the value lives.
		 */
		struct VMValue {
			base::CRef<vm::IVMValue> val;
			tsh::SymbolType<>        type;
		};

		/**
		 * @brief A tuple whose type is compatible with the meta type, i.e. it can be lifted to a
		 * type. Its elements are types, units or nested type tuples.
		 * @note This is an optimization: such tuples are kept structured, so they can be lifted to
		 * a type without the VM. Every other tuple value is represented as a VMValue.
		 */
		struct TypeTuple {
			using Element = std::variant<TypeTuple, tsh::SymbolType<>, UnitCTV>;

			explicit TypeTuple(std::vector<Element> elements);

			/**
			 * @brief Checks if a value of `tuple_type` can be represented as a TypeTuple.
			 * @note Only direct (not referenced) tuples are accepted.
			 */
			[[nodiscard]]
			static bool isValid(query::Context& ctx, const tsh::SymbolType<>& tuple_type);

			/**
			 * @brief Converts `ctv` to an Element.
			 * @note Panics if `ctv` is not a type, a unit or a type tuple.
			 */
			[[nodiscard]]
			static Element elementFromCtv(const CompileTimeValue& ctv);

			/**
			 * @brief Converts `element` to a CTV.
			 */
			[[nodiscard]]
			static CompileTimeValue elementToCtv(const Element& element);

			/**
			 * @brief Get the elements of the tuple CTV.
			 * @return The vector of the tuple CTV's elements.
			 */
			[[nodiscard]]
			const std::vector<Element>& getElements() const {
				return elements;
			}

		private:
			std::vector<Element> elements;
		};

	private:
		using Storage = std::variant<
			bool,
			NumericValue,
			char,
			CharSliceValue,
			StringClassValue,
			UnitCTV,
			TypeTuple,
			tsh::SymbolType<>,
			VMValue>;
		Storage value;

	public:
		CompileTimeValue() = default;

		/**
		 * @brief Template constructor of CTV for all types which exist in the Storage variant.
		 */
		template<typename T>
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>)
		constexpr CompileTimeValue(T val): value(std::move(val)) {}

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
		[[nodiscard]] constexpr base::Optional<T> get() const {
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
		requires(base::IS_VARIANT_MEMBER_V<T, Storage>) [[nodiscard]] constexpr bool has() const {
			return std::holds_alternative<T>(value);
		}

		/**
		 * @brief Returns the compiler::tsh::SymbolType based on the value stored in the CTV.
		 * @param ctx The query context for lifting unit value to unit type.
		 * @return The type of the value stored in the CTV.
		 */
		[[nodiscard]] tsh::SymbolType<> getTypeOfStoredValue(query::Context& ctx) const;

		/**
		 * @brief Computes a hash identifying the stored value within a single compilation.
		 * @note VMValue CTVs are hashed by their type and raw bytes, pointers inside are hashed as
		 * they are, without following them.
		 */
		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};
}
