#include "raw_view.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/shared_view.hpp>

#include <cstring>

namespace base {
	OwningView RawView::memoryCopy() const {
		auto new_data = new byte[arr_size];
		std::memcpy(new_data, begin, arr_size);
		return { new_data, arr_size };
	}

	RawView RawView::subSuffix(usize from) const { return { begin + from, arr_size - from }; }

	RawView::RawView(const char* const c_str): begin{ reinterpret_cast<RawArray>(c_str) } {
		i32 pos = 0;
		while (c_str[pos] != '\0') {
			arr_size++;
			pos++;
		}
	}

	std::string_view RawView::stringView() const {
		return { reinterpret_cast<const char*>(begin), arr_size };
	}

	std::string RawView::stdString() const {
		return std::string(std::string_view(reinterpret_cast<const char*>(begin), arr_size));
	}

	bool RawView::operator==(const RawView& oth) const { return stringView() == oth.stringView(); }

	usize RawView::size() const { return arr_size; }

	byte RawView::operator[](usize index) const { return begin[index]; }

	RawArray RawView::getBegin() const { return begin; }

	const RawView SharedView::view() const {
		CORE_ASSERT(content, "SharedView is empty");
		return content->view();
	}
}
