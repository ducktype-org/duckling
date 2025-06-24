#pragma once

#include <base/raw_view.hpp>

#include <memory>
#include <utility>

namespace fs {
	class FileContent {
		std::shared_ptr<base::OwningView> content;
		friend class File;

		explicit FileContent(std::shared_ptr<base::OwningView> content):
			  content(std::move(content)) {}

	public:
		FileContent(): content(nullptr) {}

		FileContent(const FileContent&) = default;
		FileContent(FileContent&&)      = default;

		FileContent& operator=(const FileContent&) = default;
		FileContent& operator=(FileContent&&)      = default;

		static FileContent fromString(std::string_view str) {
			return FileContent(std::make_shared<base::OwningView>(str.data()));
		}

		usize size() { return view().size(); }

		byte operator[](usize i) { return view()[i]; }

		base::RawView view() { return content->view(); }

		[[nodiscard]]
		const base::RawView view() const {
			return content->view();
		}
	};
}
