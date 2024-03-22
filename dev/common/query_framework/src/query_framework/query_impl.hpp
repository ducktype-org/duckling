#pragma once

#include <base/optional.hpp>
#include <base/str_concat.hpp>

#include "acd.hpp"
#include "query_int.hpp"
#include "dep_graph.hpp"
#include "logs.hpp"

namespace query {

	namespace detail {
		struct ContextType {

			NodeID my_node;

			template<typename OthQuery>
			auto query(typename OthQuery::QKey key) -> auto {
				
				NodeID dep_id = makeNodeID(OthQuery::id, key);
				dep_graph::addDependency(my_node, dep_id);

				return OthQuery::internal_query(key, my_node);
			}

			void log(std::string_view str) {
				// @TODO: arguments of this function should be evaluated only if logging is enabled
				::query::log("[USER LOG]: ");
				::query::log(str);
			}

			// @FUTURE: this function should take some diagnostic object as a parameter
			// Trivial implementation for now
			void compilationError(std::string_view error);
		};

		template<typename QueryImplType>
		auto standardQueryEntry(typename QueryImplType::QKey key, NodeID from) -> QueryImplType::QResult {
			log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Enter.\n"));
			
			if (auto v = QueryImplType::load(key)) {
				// @FUTURE: Add ACD check here...
				log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Cached. Done.\n"));
				return v.value().data;
			}
			else {
				auto node_id = makeNodeID(QueryImplType::QueryType::id, key);
				typename QueryImplType::Context context{node_id};
				ACD acd; /*@TODO: provide acd here*/

				// epilog:
				dep_graph::setEntry(node_id, from);
				log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Calculating.\n"));
				
				// calculation:
				auto result = QueryImplType::store(key, QueryImplType::provide(context, key), acd);
				
				// prolog:
				dep_graph::setExit(node_id);
				log(base::strConcat("[QUERY \"", QueryImplType::QueryType::name, "\"]: Done.\n"));
				
				return result;
			}
		}

	}

	template<
		typename QueryType_tp,
		typename PResult_tp
	>
	struct QueryImplementation {
		using QueryType = QueryType_tp;
		using QKey = typename QueryType_tp::QKey;
		
		using QResult = typename QueryType_tp::QResult;
		using PResult = PResult_tp;
		
		using Context = ::query::detail::ContextType;

		using QResWithACD = AddACD<QResult>;

		using LoadRes = base::Optional<QResWithACD>;

		// static auto provide(QKey, Context&) -> PResult;
		// static auto load(QKey key) -> base::Optional<QResult>;
		// static auto store(QKey key, PResult) -> QResult;
	};

}

#define QUERY_IMPLEMENTATION_BOILERPLATE(type, pretty_name) \
	auto type::QueryType::internal_query(type::QueryType::QKey key, ::query::NodeID from) -> type::QueryType::QResult { \
 		return ::query::detail::standardQueryEntry<type>(key, from);                     \
	}                                                                                    \
	decltype(type::QueryType::id) type::QueryType::id = ::query::nextQueryId();          \
	decltype(type::QueryType::name) type::QueryType::name = pretty_name;


// @TODO: why this has to be here..?
template <>
struct std::hash<::query::EmptyKey> {
	std::size_t operator()([[maybe_unused]] const ::query::EmptyKey& key) const {
		return 0;
	}
};