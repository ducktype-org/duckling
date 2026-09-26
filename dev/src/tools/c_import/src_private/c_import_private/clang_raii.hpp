#pragma once

#include <clang-c/Index.h>

#include <string>
#include <utility>

namespace c_import::detail {

	/**
	 * @brief Owns a CXString.
	 *
	 * Every libclang handle is wrapped here so the rest of the tool never touches the C API
	 * directly, which also keeps the required NOLINTs in one place.
	 */
	class OwnedString final {
	public:
		explicit OwnedString(CXString string): m_string(string) {}

		OwnedString(const OwnedString&)            = delete;
		OwnedString& operator=(const OwnedString&) = delete;
		OwnedString(OwnedString&&)                 = delete;
		OwnedString& operator=(OwnedString&&)      = delete;

		~OwnedString() { clang_disposeString(m_string); }

		[[nodiscard]] std::string str() const {
			const char* text = clang_getCString(m_string);
			return text == nullptr ? std::string{} : std::string(text);
		}

	private:
		CXString m_string;
	};

	inline std::string spellingOf(CXCursor cursor) {
		return OwnedString(clang_getCursorSpelling(cursor)).str();
	}

	inline std::string spellingOf(CXType type) {
		return OwnedString(clang_getTypeSpelling(type)).str();
	}

	class OwnedIndex final {
	public:
		OwnedIndex(): m_index(clang_createIndex(0, 0)) {}

		OwnedIndex(const OwnedIndex&)            = delete;
		OwnedIndex& operator=(const OwnedIndex&) = delete;
		OwnedIndex(OwnedIndex&&)                 = delete;
		OwnedIndex& operator=(OwnedIndex&&)      = delete;

		~OwnedIndex() { clang_disposeIndex(m_index); }

		[[nodiscard]] CXIndex get() const { return m_index; }

	private:
		CXIndex m_index;
	};

	class OwnedTranslationUnit final {
	public:
		explicit OwnedTranslationUnit(CXTranslationUnit unit): m_unit(unit) {}

		OwnedTranslationUnit(const OwnedTranslationUnit&)            = delete;
		OwnedTranslationUnit& operator=(const OwnedTranslationUnit&) = delete;
		OwnedTranslationUnit(OwnedTranslationUnit&&)                 = delete;
		OwnedTranslationUnit& operator=(OwnedTranslationUnit&&)      = delete;

		~OwnedTranslationUnit() {
			if (m_unit != nullptr) clang_disposeTranslationUnit(m_unit);
		}

		[[nodiscard]] CXTranslationUnit get() const { return m_unit; }

		[[nodiscard]] bool valid() const { return m_unit != nullptr; }

	private:
		CXTranslationUnit m_unit;
	};

}
