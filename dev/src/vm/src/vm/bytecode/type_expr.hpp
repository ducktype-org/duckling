// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file type_expr.hpp
 * @brief A reference to a type in the bytecode, which is either a name or a type built from
 * another type, such as `ptr{i64}` or `dyntable{i64}`.
 *
 * Derived types are not declared. Every spelling of `ptr{Foo}` denotes the same type, so the
 * VM can intern one such type per argument instead of letting two declarations describe the
 * same thing.
 */

#pragma once

#include <base/collections/optional.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <variant>

namespace vm::code {
	namespace expr {
		struct Named;
		struct Pointer;
		struct CPointer;
		struct DynamicTable;
	}

	namespace detail {
		struct TypeExprNode;
	}

	/**
	 * @brief A type reference, as written in the bytecode.
	 *
	 * @note This is a handle into a process-wide pool of interned nodes, in the same way that
	 * `base::StrID` is a handle into a pool of interned strings. It is therefore cheap to copy
	 * and compare, and equal expressions always share one node, so `operator==` is structural
	 * equality. Nodes live until the process exits.
	 */
	class TypeExpr final {
	public:
		/**
		 * @brief Builds an invalid expression, which denotes no type.
		 * @note Present because the bytecode structures holding a `TypeExpr` are default
		 * constructed and then filled in, the same reason `base::StrID` has a default.
		 * Every accessor rejects it.
		 */
		TypeExpr() = default;

		/**
		 * @brief The expression for a declared type's name, such as `Dog`.
		 * @note Interning in the constructor, the way `base::StrID` does, is what makes equal
		 * expressions share one node.
		 */
		explicit TypeExpr(base::StrID name);

		explicit TypeExpr(const expr::Named& named);

		explicit TypeExpr(const expr::Pointer& pointer);

		explicit TypeExpr(const expr::CPointer& cpointer);

		explicit TypeExpr(const expr::DynamicTable& dynamic_table);

		/// @brief The expression for a declared type's name, such as `Dog`.
		[[nodiscard]] static TypeExpr type(base::StrID name);

		/// @brief A managed pointer to `pointee`, such as `ptr{Dog}`.
		[[nodiscard]] static TypeExpr pointer(TypeExpr pointee);

		/// @brief A C pointer, such as `cptr{Dog}`. An absent pointee gives `cptr{}`.
		[[nodiscard]] static TypeExpr cpointer(const base::Optional<TypeExpr>& pointee = {});

		/// @brief A dynamic table of `element`, such as `dyntable{i64}`.
		[[nodiscard]] static TypeExpr dyntable(TypeExpr element);

		/// @brief Whether this expression denotes a type, i.e. was not default constructed.
		[[nodiscard]] bool isValid() const noexcept { return node != nullptr; }

		/// @brief Which form this expression takes, for `variant_match`.
		[[nodiscard]] const std::variant<expr::Named, expr::Pointer, expr::CPointer, expr::DynamicTable>& getKind(
		) const;

		template<class T>
		[[nodiscard]] bool isKind() const;

		template<class T>
		[[nodiscard]] base::Optional<CRef<T>> maybeGetKindAs() const;

		template<class T>
		[[nodiscard]] CRef<T> getKindAs() const;

		/**
		 * @brief The spelling of this expression, which is also the name of the type it denotes.
		 * @note Computed once per node, so that a serialized expression and the name of the
		 * interned type can never disagree.
		 */
		[[nodiscard]] base::StrID canonicalName() const;

		/// @note Interning makes this a pointer comparison.
		bool operator==(const TypeExpr& other) const noexcept { return node == other.node; }

		/// @note Hashes the order the node was interned in rather than its address, as
		/// `base::StrID` does, so that it does not change with where the node was allocated.
		[[nodiscard]] u64 hash() const noexcept;

	private:
		const detail::TypeExprNode* node = nullptr;
	};

	static_assert(
		std::is_trivially_copyable_v<TypeExpr> && std::is_trivially_destructible_v<TypeExpr>,
		"TypeExpr is stored in instruction arguments, which must stay trivially copyable"
	);

	/// @brief The forms a type expression can take.
	namespace expr {
		/// @brief A type referred to by its declared name, such as `i64` or `Dog`.
		struct Named final {
			base::StrID name;
		};

		/// @brief A managed pointer, `ptr{T}`.
		struct Pointer final {
			TypeExpr pointee;
		};

		/// @brief A raw native address, `cptr{T}`, or `cptr{}` when the pointee is unknown
		/// (C's `void*`).
		struct CPointer final {
			base::Optional<TypeExpr> pointee;
		};

		/// @brief A table whose length is only known at run time, `dyntable{T}`.
		struct DynamicTable final {
			TypeExpr element;
		};

		using Variant = std::variant<Named, Pointer, CPointer, DynamicTable>;

		template<class T>
		concept ConcreteExpr = base::IsOneOf<T, Named, Pointer, CPointer, DynamicTable>;
	}

	template<class T>
	bool TypeExpr::isKind() const {
		static_assert(expr::ConcreteExpr<T>, "Not a form a type expression can take");
		return std::holds_alternative<T>(getKind());
	}

	template<class T>
	base::Optional<CRef<T>> TypeExpr::maybeGetKindAs() const {
		static_assert(expr::ConcreteExpr<T>, "Not a form a type expression can take");
		if (!isKind<T>()) return {};
		return &std::get<T>(getKind());
	}

	template<class T>
	CRef<T> TypeExpr::getKindAs() const {
		return maybeGetKindAs<T>().expect("Tried to get a type expression as the wrong form");
	}
}

template<>
struct std::hash<vm::code::TypeExpr> {
	std::size_t operator()(const vm::code::TypeExpr& expr) const noexcept { return expr.hash(); }
};
