#pragma once

#include <base/except/exceptions.hpp>                            // IWYU pragma: export

#include <query_framework/context/context.hpp>                   // IWYU pragma: export
#include <query_framework/internal/context_access.hpp>           // IWYU pragma: export
#include <query_framework/internal/query_data/query_id.hpp>      // IWYU pragma: export
#include <query_framework/internal/query_graph/node_making.hpp>  // IWYU pragma: export
#include <query_framework/utils/query_hash.hpp>                  // IWYU pragma: export

/**
 * @brief Implements a side-input query.
 * @note We don't care here about active graph, since inputs have no dependencies.
 * @TODO: #1887 make it clear what query invocation layers happen here.
 */
#define IMPLEMENT_QUERY_SIDE_INPUT(query_type)                                                \
	auto query_type::internal_query(const query_type::QKey& key) -> query_type::QResult {     \
		auto node_id = ::query::internal::makeNodeID<query_type>(key);                        \
		::query::internal::ContextAccess::getState()->addSideInputNode(node_id);              \
		return ::query::internal::SideInputMockValue{};                                       \
	}                                                                                         \
	auto query_type::internal_load(::query::QueryStableHash) -> query_type::QResult {         \
		CORE_UNREACHABLE();                                                                   \
	}                                                                                         \
	auto query_type::internal_erase(::query::QueryStableHash) -> bool { CORE_UNREACHABLE(); } \
	static_assert(                                                                            \
		not std::is_reference_v<query_type::QKey>,                                            \
		"Query key type should not be a reference (use custom struct instead)"                \
	);                                                                                        \
	static_assert(                                                                            \
		::query::HasStablePerfectHash<query_type::QKey>,                                      \
		"queryStablePerfectHash must be implemented for side inputs keys"                     \
	);                                                                                        \
	static_assert(query_type::QueryType::QUERY_DATA.verify(), "Query data is inconsistent."); \
	decltype(query_type::id) query_type::id                                                   \
		= ::query::internal::registerQuery(query_type::QUERY_DATA);

/**
 * @brief Implement a side input query with custom evaluate logic.
 *
 * Use when you need to execute additional logic (e.g., adding metadata) when the side input
 * is accessed. You must specify names for the context and key parameters explicitly.
 *
 * @note The provided evaluate function will be called every query call so it can be called multiple
 * times during compilation.
 *
 * @param query_type  The side input query type (declared with DECLARE_QUERY_SIDE_INPUT)
 * @param ctx_name    Name for the query::Context& parameter (use it in evaluate_body)
 * @param key_name    Name for the QKey parameter (use it in evaluate_body)
 * @param evaluate_body Code block executed when side input is accessed
 *
 * Example:
 * @code
 * IMPLEMENT_QUERY_SIDE_INPUT_WITH_LOGIC(QueryModuleChildSideInput, ctx, key, {
 *     ctx.addMetadataIfNotExists<metadata_ModuleLookup>(key);
 * });
 * @endcode
 */
#define IMPLEMENT_QUERY_SIDE_INPUT_WITH_LOGIC(query_type, ctx_name, key_name, evaluate_body)   \
	auto query_type::internal_query(const query_type::QKey& key_name) -> query_type::QResult { \
		auto node_id  = ::query::internal::makeNodeID<query_type>(key_name);                   \
		auto ctx_name = ::query::internal::ContextAccess::make(node_id);                       \
		::query::internal::ContextAccess::getState()->addSideInputNode(node_id);               \
		evaluate_body return ::query::internal::SideInputMockValue{};                          \
	}                                                                                          \
	auto query_type::internal_erase(::query::QueryStableHash) -> bool { return false; }        \
	static_assert(                                                                             \
		not std::is_reference_v<query_type::QKey>,                                             \
		"Query key type should not be a reference (use custom struct instead)"                 \
	);                                                                                         \
	static_assert(                                                                             \
		::query::HasStablePerfectHash<query_type::QKey>,                                       \
		"queryStablePerfectHash must be implemented for side inputs keys"                      \
	);                                                                                         \
	static_assert(query_type::QueryType::QUERY_DATA.verify(), "Query data is inconsistent.");  \
	decltype(query_type::id) query_type::id                                                    \
		= ::query::internal::registerQuery(query_type::QUERY_DATA);
