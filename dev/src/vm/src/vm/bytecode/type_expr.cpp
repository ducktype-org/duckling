// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file type_expr.cpp
 * @brief The pool of interned type expressions.
 *
 * Built like the `base::StrID` interner: a sharded concurrent map, so that a lookup never takes
 * the insertion mutex, and nodes that are never freed. The compiler lowers to bytecode on several
 * query workers at once, so expressions are built concurrently.
 */

#include <concurrent/base/collections/hash_map.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <vm/bytecode/type_expr.hpp>

#include <algorithm>
#include <deque>
#include <mutex>
#include <string>

namespace vm::code {
	namespace detail {
		struct TypeExprNode final {
			expr::Kind kind;
			/// The spelling of this node, which doubles as the name of the type it denotes.
			base::StrID canonical_name;
			/// The order this node was interned in.
			u64 id;
		};
	}

	namespace {
		using detail::TypeExprNode;

		/// Nodes are keyed by their spelling, which identifies an expression on its own.
		using NodeMap = concurrent::ConHashMap<base::StrID, const TypeExprNode*>;

		Ref<NodeMap> getNodeMap() {
			// Not `constinit`, because the map has no constant-expression constructor.
			static NodeMap node_map;
			return &node_map;
		}

		/// Nodes are immortal and must keep their addresses, because a handle is a raw pointer.
		std::deque<TypeExprNode>& nodeStorage() {
			static std::deque<TypeExprNode> storage;
			return storage;
		}

		constinit std::mutex mutex;

		base::StrID buildCanonicalName(const expr::Kind& kind) {
			variant_match(kind) {
				variant_case(expr::Named, named) { return named.name; }
				variant_case(expr::Pointer, pointer) {
					return base::StrID("ptr{" + pointer.pointee.canonicalName().str() + "}");
				}
				variant_case(expr::CPointer, cpointer) {
					if (cpointer.pointee.empty()) return base::StrID("cptr{}");
					return base::StrID("cptr{" + cpointer.pointee->canonicalName().str() + "}");
				}
				variant_case(expr::DynamicTable, dynamic_table) {
					return base::StrID(
						"dyntable{" + dynamic_table.element.canonicalName().str() + "}"
					);
				}
				variant_case(expr::FixedSizeTable, fixed_size_table) {
					return base::StrID(base::strConcat(
						"fixtable{",
						fixed_size_table.element.canonicalName().strView(),
						", ",
						fixed_size_table.length,
						"}"
					));
				}
				variant_case(expr::Variant, variant) {
					std::string spelling = "variant{";
					for (usize i = 0; i < variant.alternatives.size(); ++i) {
						if (i != 0) spelling += ", ";
						spelling += variant.alternatives[i].canonicalName().strView();
					}
					return base::StrID(spelling + "}");
				}
			}
			CORE_UNREACHABLE();
		}

		const TypeExprNode* intern(const expr::Kind& kind) {
			const base::StrID canonical_name = buildCanonicalName(kind);

			// Fast path: an existing node is found without taking the insertion mutex.
			if (auto node = getNodeMap()->atMaybeCopy(canonical_name)) return *node;

			std::scoped_lock lock(mutex);

			// Another thread may have inserted the node between the two lookups.
			if (auto node = getNodeMap()->atMaybeCopy(canonical_name)) return *node;

			const TypeExprNode* node = &nodeStorage().emplace_back(TypeExprNode{
				.kind = kind, .canonical_name = canonical_name, .id = nodeStorage().size() });
			getNodeMap()->put(base::StrID(canonical_name), node);
			return node;
		}
	}

	TypeExpr::TypeExpr(base::StrID name): TypeExpr(expr::Named{ .name = name }) {}

	TypeExpr::TypeExpr(const expr::Named& named) {
		// A name is what its expression is interned under, so one spelled like a derived
		// expression would be the same node as it; an empty one would make `cptr{}` of it.
		const std::string_view name = named.name.strView();
		if (name.empty() || name.find_first_of("{} \t") != std::string_view::npos)
			CORE_PANIC("Not a valid type name: `", name, "`");
		node = intern(named);
	}

	TypeExpr::TypeExpr(const expr::Pointer& pointer) {
		CORE_ASSERT(pointer.pointee.isValid(), "A managed pointer needs a pointee");
		node = intern(pointer);
	}

	TypeExpr::TypeExpr(const expr::CPointer& cpointer) {
		CORE_ASSERT(
			cpointer.pointee.empty() || cpointer.pointee->isValid(),
			"A C pointer either has a pointee or has none at all"
		);
		node = intern(cpointer);
	}

	TypeExpr::TypeExpr(const expr::DynamicTable& dynamic_table) {
		CORE_ASSERT(dynamic_table.element.isValid(), "A dynamic table needs an element type");
		node = intern(dynamic_table);
	}

	TypeExpr::TypeExpr(const expr::FixedSizeTable& fixed_size_table) {
		CORE_ASSERT(fixed_size_table.element.isValid(), "A fixed-size table needs an element type");
		node = intern(fixed_size_table);
	}

	TypeExpr::TypeExpr(const expr::Variant& variant) {
		CORE_ASSERT(
			std::ranges::all_of(variant.alternatives, &TypeExpr::isValid),
			"Every variant alternative must denote a type"
		);
		node = intern(variant);
	}

	TypeExpr TypeExpr::type(base::StrID name) { return TypeExpr(expr::Named{ .name = name }); }

	TypeExpr TypeExpr::pointer(TypeExpr pointee) {
		return TypeExpr(expr::Pointer{ .pointee = pointee });
	}

	TypeExpr TypeExpr::cpointer(const base::Optional<TypeExpr>& pointee) {
		return TypeExpr(expr::CPointer{ .pointee = pointee });
	}

	TypeExpr TypeExpr::dyntable(TypeExpr element) {
		return TypeExpr(expr::DynamicTable{ .element = element });
	}

	TypeExpr TypeExpr::fixtable(TypeExpr element, usize length) {
		return TypeExpr(expr::FixedSizeTable{ .element = element, .length = length });
	}

	TypeExpr TypeExpr::variant(std::vector<TypeExpr> alternatives) {
		return TypeExpr(expr::Variant{ .alternatives = std::move(alternatives) });
	}

	const expr::Kind& TypeExpr::getKind() const {
		CORE_ASSERT(isValid(), "Type expression denotes no type");
		return node->kind;
	}

	u64 TypeExpr::hash() const noexcept { return node == nullptr ? 0 : node->id + 1; }

	base::StrID TypeExpr::canonicalName() const {
		CORE_ASSERT(isValid(), "Type expression denotes no type");
		return node->canonical_name;
	}
}
