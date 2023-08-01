#include "../string_id.hpp"
#include <iostream>
#include <cstring>

namespace base {

	StrId::ToDataType StrId::to_data_map;
	StrId::ToIdType StrId::to_id_map;

	constexpr size_t default_buffer_size = 32768;
	typedef std::vector<base::OwningView> BufferList;
	namespace {
		BufferList buffer_list;

		// remanding size of last buffer (equals default_buffer_size - next_pos)
		size_t size_left = 0;

		// next free position in last buffer
		size_t next_pos = 0;

		bool any_buffer_exits = false;
	}

	void newBuffer() {
		auto new_buffer = new byte[default_buffer_size];
		buffer_list.emplace_back(new_buffer, default_buffer_size);
		size_left = default_buffer_size;
		next_pos = 0;
	}

	base::RawView lastBuffer() {
		return buffer_list.back().view();
	}

	/**
	 * @brief 
	 * Stores data is vector of buffers of size 32768
	 * New buffer is created, when new string cannot fit
	 * in previous one.
	 * If string has length greater then 32768 it is given its own buffer.
	 * Only last buffer is considered
	 * 
	 * @OPT: better memory/buffers usage
	 */
	StrId::StrId(const base::RawView& data) {
		RIFT_ASSERT(data.getBegin() != nullptr, "StrId received null string");

		if (to_id_map.contains(data)) {
			id = to_id_map[data];
			return;
		}

		id = InnerId::next();

		base::RawView actual_data;

		// @TODO: add test to this:
		if (data.size() > default_buffer_size) {
			// Data is too big to fit into any buffer
			buffer_list.emplace_back(base::OwningView::copy(data));
			actual_data = RawView(lastBuffer().getBegin(), data.size());
			any_buffer_exits = true;
		}
		else {
			if (size_left < data.size() || !any_buffer_exits) {
				// Data can't fit into last buffer
				newBuffer();
				any_buffer_exits = true;
			}

			actual_data = RawView(lastBuffer().getBegin() + next_pos, data.size());
			std::memcpy(buffer_list.back().begin + next_pos, data.getBegin(), data.size());
			size_left -= data.size();
			next_pos += data.size();
		}

		to_data_map.put(id, actual_data);
		to_id_map.put(actual_data, id);
	}

	StrId::StrId(const char* data) :
			StrId(base::RawView(data)) {}

	StrId::StrId(char character): StrId(std::string(1, character).c_str()) {}

	void StrId::dumpData(std::ostream& out) {
		int i = 0;
		for (auto v: to_data_map) {
			if (v) {
				out << i << ": " << v->stringView() << "\n";
			}
			i++;
		}
	}
}
