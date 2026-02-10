#include "string_id.hpp"

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

#include <cstring>
#include <iostream>
#include <mutex>
#include <shared_mutex>

namespace base {
	/**
	 * Size of memory buffers used to store byte-strings represented by StrID
	 */
	constexpr usize DEFAULT_BUFFER_SIZE = 32'768;

	namespace {
		using BufferList = std::vector<base::OwningView>;
		using ToDataType = VectorMap<StrID::InnerID, RawView>;
		using ToIDType   = HashMap<RawView, StrID::InnerID>;

		Ref<ToDataType> getToDataMap() {
			static ToDataType to_data_map;
			return &to_data_map;
		}

		Ref<ToIDType> getToIDMap() {
			static ToIDType to_id_map;
			return &to_id_map;
		}

		Ref<BufferList> getBufferList() {
			static BufferList buffer_list;
			return &buffer_list;
		}

		// remaining size of last buffer (equals default_buffer_size - next_pos)
		usize size_left = 0;

		// next free position in last buffer
		usize next_pos = 0;

		bool any_buffer_exits = false;

		std::shared_mutex mutex;
	}

	/**
	 * @brief Creates a new buffer for storing strings.
	 * @note It is required to be called under write-lock.
	 */
	void newBuffer() {
		auto new_buffer = new byte[DEFAULT_BUFFER_SIZE];
		getBufferList()->emplace_back(new_buffer, DEFAULT_BUFFER_SIZE);
		size_left = DEFAULT_BUFFER_SIZE;
		next_pos  = 0;
	}

	/**
	 * @brief Returns a view to the last buffer.
	 * @note It is required to be called under read-lock.
	 */
	base::RawView lastBuffer() { return getBufferList()->back().view(); }

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
	StrID::StrID(const base::RawView& data) {
		CORE_ASSERT(data.getBegin() != nullptr, "StrID received null string");

		std::unique_lock lock(mutex);

		if (auto id = getToIDMap()->atMaybe(data)) {
			this->id = **id;
			return;
		}

		id = InnerID::next();

		base::RawView actual_data;

		// @TODO: add test to this:
		if (data.size() > DEFAULT_BUFFER_SIZE) {
			// Data is too big to fit into any buffer
			getBufferList()->emplace_back(base::OwningView::copy(data));
			actual_data      = RawView(lastBuffer().getBegin(), data.size());
			any_buffer_exits = true;
		} else {
			if (size_left < data.size() || !any_buffer_exits) {
				// Data can't fit into last buffer
				newBuffer();
				any_buffer_exits = true;
			}

			actual_data = RawView(lastBuffer().getBegin() + next_pos, data.size());
			std::memcpy(getBufferList()->back().begin + next_pos, data.getBegin(), data.size());
			size_left -= data.size();
			next_pos += data.size();
		}

		getToDataMap()->put(id, actual_data);
		getToIDMap()->put(actual_data, id);
	}

	StrID::StrID(const char* data): StrID(base::RawView(data)) {}

	StrID::StrID(const std::string& data): StrID(data.c_str()) {}

	StrID::StrID(char character): StrID(std::string(1, character).c_str()) {}

	base::RawView StrID::view() const {
		CORE_ASSERT(id.isGood(), "StrID is bad");
		std::shared_lock lock(mutex);
		return getToDataMap()->operator[](id);
	}

	void StrID::dumpData(std::ostream& out) {
		i32              i = 0;
		std::shared_lock lock(mutex);
		for (auto v: *getToDataMap()) {
			if (v) out << i << ": " << v->stringView() << "\n";
			i++;
		}
	}

}
