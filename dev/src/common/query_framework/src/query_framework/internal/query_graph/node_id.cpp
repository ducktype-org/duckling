#include "node_id.hpp"

#include <concurrent/base/collections/hash_map.hpp>

namespace query::internal {
	static concurrent::ConHashMap<NodeID, std::string> node_debug_name;

	base::Optional<std::string_view> NodeID::debugString() const {
		return node_debug_name.atMaybe(*this).map([](auto ref_str) -> std::string_view {
			return *ref_str;
		});
	}

	void NodeID::setDebugString(std::string str) const {
		node_debug_name.maybePut(*this, std::move(str));
	}

	concurrent::ConHashMap<NodeID, bool> node_results;

	void NodeID::setResult(bool success) const { node_results.maybePut(*this, success); }

	base::Optional<bool> NodeID::isSuccess() const {
		if_opt_some(node_results.atMaybe(*this), val) { return *val; }
		return {};
	}

	std::string NodeID::hashString() const { return std::to_string(q_id.asInt()) + "::" + hash.val.toStringHex(); }
}
