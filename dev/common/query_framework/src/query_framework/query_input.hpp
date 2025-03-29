/**
 * @file query_int.hpp
 * @brief Implementation of automatic generation of query inputs.
 */

#pragma once

#include "query_impl.hpp" // IWYU pragma: export


namespace query::detail {

    struct SideInputMockValue { };

}

/**
 * @brief Macro used do delcare queries side-inputs.
 * This is different from standards query input, in a way that input
 * it sort of artificially added. 
 * PST currently use it to mark a query input of given PST node,
 * even tho PST creation is not query based.
 */
#define DECLARE_QUERY_SIDE_INPUT(query_type, key) \
    DECLARE_QUERY_AUX(query_type, key, value, true)

#define IMPLEMENT_QUERY_SIDE_INPUT_AUX(query_type) \
        auto query_type::internal_query(type::QKey key, ::query::detail::NodeID from)              \
        -> type::QResult {                                                                          \
            auto node_id = query::detail::makeNodeID(query_type::id, key); \
            dep_graph::addDependency(my_node, node_id);
            dep_graph::setEntry(node_id, from);
        }                                                                                               \
        decltype(query_type::id)   query_type::id = ::query::detail::newQueryID(pretty_name); \
        decltype(query_type::name) query_type::name = #query_type;

// Some side notes:
// * query input : query without key, of which value might change depending on outside world – pointer size, compilation options
// * query output: query with key, of which value might change depending on outside world ? 
// * query side input: query with key, without output – or with some dummy output?

// There is still an idea of key-dependencies