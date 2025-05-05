/**
 * Definition of query Context type.
 */

#pragma once

#include "context_fd.hpp"  // IWYU pragma: keep
#include "detail/query_graph/dep_graph.hpp"
#include "detail/query_graph/node_id.hpp"
#include "detail/query_graph/node_making.hpp"  // IWYU pragma: export

#include <diagnostic/logger.hpp>
#include <diagnostic/message.hpp>

#include <base/defer.hpp>

namespace query {

	namespace detail {
		struct ContextMaker;
	}

	/**
	 * @brief Context is type of a special object
	 * that query implementation use to perform three key operations:
	 * 	* call other query
	 *  * log
	 *  * report compiler error
	 *
	 * @FUTURE: there exist a concept of "custom context" types as
	 * a way to hack-in the query model. This however will most likely be
	 * discarded.
	 */
	struct Context final {
	private:
		detail::NodeID my_node;
		bool           active = true;

		Context(detail::NodeID my_node): my_node(my_node) {}
		friend struct query::detail::ContextMaker;

	public:
		// @TODO: Make the context (and thus the logger) be propagated through query calls,
		// so that all queries run on the same file / in the same compilation thread / whatever
		// use a single, *non-static* logger object.
		static dia::Logger logger;

		Context(const Context&) = delete;
		Context(Context&&)      = delete;

		void assertActive() const { CORE_ASSERT(active, "Context is inactive."); }

		template<typename OthQuery>
		auto query(typename OthQuery::QKey key) -> decltype(auto) {
			assertActive();
			detail::NodeID dep_id = makeNodeID(OthQuery::id, key);
			detail::dep_graph::addDependency(my_node, dep_id);

			this->active = false;
			defer(this->active = true);

			return OthQuery::internal_query(key, my_node);
		}

		/**
		 * Log message to be shown to the user.
		 * @param message The dia::Message to be logged.
		 */
		void log(Box<dia::Message> message);
	};
}
