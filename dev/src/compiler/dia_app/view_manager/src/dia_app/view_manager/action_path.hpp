#pragma once
#include <cstddef>
#include <vector>

namespace dia {
	class ActionPath {
		const std::vector<size_t>& path;
		const size_t               action_id;
		// Current index on the component path.
		size_t cur_idx;

	public:
		ActionPath(const std::vector<size_t>& path, size_t action_id):
			  path(path),
			  action_id(action_id),
			  cur_idx(0) {}

		bool path_end() const { return cur_idx >= path.size(); }

		size_t next() { return path[cur_idx++]; }

		size_t action() const { return action_id; }
	};
}  // namespace dia
