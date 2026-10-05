// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "string_id.hpp"

#include <concurrent/base/collections/hash_map.hpp>

#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

#include <cstring>
#include <iostream>
#include <mutex>
#include <vector>

ID_STD_HASH(base::internal::StrInnerID);

namespace base {
	/**
	 * Size of memory buffers used to store byte-strings represented by StrID
	 */
	constexpr usize DEFAULT_BUFFER_SIZE = 32'768;

	namespace {
		using BufferList = std::vector<base::OwningView>;
		using ToDataType = concurrent::ConHashMap<StrID::InnerID, RawView>;
		using ToIDType   = concurrent::ConHashMap<RawView, StrID::InnerID>;

		Ref<ToIDType> getToIDMap() {
			// This does not have a constinit constructor
			static ToIDType to_id_map;
			return &to_id_map;
		}

		Ref<ToDataType> getToDataMap() {
			// This does not have a constinit constructor
			static ToDataType to_data_map;
			return &to_data_map;
		}

		constinit BufferList buffer_list;

		// remaining size of last buffer (equals default_buffer_size - next_pos)
		constinit usize size_left = 0;

		// next free position in last buffer
		constinit usize next_pos = 0;

		constinit bool any_buffer_exists = false;

		constinit std::mutex mutex;
	}

	/**
	 * @brief Creates a new buffer for storing strings.
	 * @note It is required to be called under write-lock.
	 */
	void newBuffer() {
		auto new_buffer = new byte[DEFAULT_BUFFER_SIZE];
		buffer_list.emplace_back(new_buffer, DEFAULT_BUFFER_SIZE);
		size_left = DEFAULT_BUFFER_SIZE;
		next_pos  = 0;
	}

	/**
	 * @brief Returns a view to the last buffer.
	 * @note It is required to be called under lock.
	 */
	base::RawView lastBuffer() { return buffer_list.back().view(); }

	/**
	 * @brief
	 * Stores data is vector of buffers of size 32768
	 * New buffer is created, when new string cannot fit
	 * in previous one.
	 * If string has length greater than 32768 it is given its own buffer.
	 * Only last buffer is considered
	 *
	 * @OPT: better memory/buffers usage
	 */
	StrID::StrID(const base::RawView& data) {
		CORE_ASSERT(data.getBegin() != nullptr, "StrID received null string");

		// Fast path: check if string already exists using just the concurrent map without local locking
		{
			if (auto id = getToIDMap()->atMaybeCopy(data)) {
				this->id = *id;
				return;
			}
		}

		std::scoped_lock lock(mutex);

		if (auto id = getToIDMap()->atMaybeCopy(data)) {
			this->id = *id;
			return;
		}

		id = InnerID::next();

		base::RawView actual_data;

		if (data.size() > DEFAULT_BUFFER_SIZE) {
			// Data is too big to fit into any buffer
			buffer_list.emplace_back(base::OwningView::copy(data));
			actual_data       = RawView(lastBuffer().getBegin(), data.size());
			any_buffer_exists = true;

			// The dedicated buffer is now the last one. The bump-allocator state
			// (next_pos/size_left) described the previous regular buffer; writing at
			// next_pos into lastBuffer() would overwrite the just-interned big string.
			// Mark the last buffer as full so the next small string opens a new one.
			next_pos  = DEFAULT_BUFFER_SIZE;
			size_left = 0;
		} else {
			if (size_left < data.size() || !any_buffer_exists) {
				// Data can't fit into last buffer
				newBuffer();
				any_buffer_exists = true;
			}

			actual_data = RawView(lastBuffer().getBegin() + next_pos, data.size());
			std::memcpy(buffer_list.back().begin + next_pos, data.getBegin(), data.size());
			size_left -= data.size();
			next_pos += data.size();
		}

		getToDataMap()->put(id, actual_data);
		getToIDMap()->put(actual_data, id);
	}

	StrID::StrID(const char* data): StrID(base::RawView(data)) {}

	StrID::StrID(char character): StrID(std::string(1, character).c_str()) {}

	StrID::StrID(std::string_view data): StrID(base::RawView{ std::as_bytes(std::span{ data }) }) {}

	base::RawView StrID::view() const {
		CORE_ASSERT(id.isGood(), "StrID is bad");
		return getToDataMap()->getCopy(id);
	}

	void StrID::dumpData(std::ostream& out) {
		i32              i = 0;
		std::scoped_lock lock(mutex);
		for (auto v: *getToDataMap()) {
			out << i << ": " << v.value.stringView() << "\n";
			i++;
		}
	}

}
