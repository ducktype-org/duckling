#pragma once

#include <base/raw_view.hpp>

#include <memory>
#include <utility>

namespace fs {
	/**
	 * @brief Stores file content.
	 * @TODO: replace with a SharedView #1034
	 * This class encapsulates the content of a file and is assumed to be immutable.
	 */
	class FileContent final {
		std::shared_ptr<base::OwningView> content;
		friend class File;

		explicit FileContent(std::shared_ptr<base::OwningView> content):
			  content(std::move(content)) {}

	public:
		FileContent(const FileContent&) = default;
		FileContent(FileContent&&)      = default;

		FileContent& operator=(const FileContent&) = default;
		FileContent& operator=(FileContent&&)      = default;

		static FileContent fromString(std::string_view str) {
			return FileContent(std::make_shared<base::OwningView>(str.data()));
		}

		usize size() { return view().size(); }

		byte operator[](usize i) const { return content->view()[i]; }

		[[nodiscard]]
		const base::RawView view() const {
			return content->view();
		}
	};
}
