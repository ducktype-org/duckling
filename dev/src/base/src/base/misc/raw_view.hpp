/**
 * @file raw_view.hpp
 * @brief Provides byte array views.
 */
#pragma once

#include <base/types/ints.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace base {
	using RawArray = const byte*;

	class OwningView;

	/**
	 * @brief Non owning immutable byte array view
	 */
	class RawView final {
	private:
		friend class OwningView;

		[[nodiscard]]
		OwningView memoryCopy() const;

		RawArray begin    = nullptr;
		usize    arr_size = 0;

	public:
		RawView()               = default;
		RawView(const RawView&) = default;
		RawView(RawView&&)      = default;

		RawView(RawArray begin, usize size): begin{ begin }, arr_size{ size } {}

		RawView& operator=(const RawView&) = default;

		// do not make explicit
		RawView(const char* c_str);

		[[nodiscard]]
		std::string_view stringView() const;
		[[nodiscard]]
		std::string stdString() const;
		[[nodiscard]]
		RawView subSuffix(usize from) const;

		[[nodiscard]]
		usize size() const;
		[[nodiscard]]
		RawArray getBegin() const;

		byte operator[](usize index) const;
		bool operator==(const RawView& oth) const;
	};

	/**
	 * @brief Non owning mutable byte array view
	 */
	class ModRawView final {
	private:
		byte* begin    = nullptr;
		usize arr_size = 0;

	public:
		ModRawView()                  = default;
		ModRawView(const ModRawView&) = default;
		ModRawView(ModRawView&&)      = default;

		ModRawView(byte* begin, usize size): begin{ begin }, arr_size{ size } {}

		ModRawView& operator=(const ModRawView&) = default;

		[[nodiscard]]
		usize size() const {
			return arr_size;
		}

		[[nodiscard]]
		byte* getBegin() const {
			return begin;
		}
	};

	class StrID;

	/**
	 * @brief Owning byte array view
	 */
	class OwningView final {
		byte* begin{ nullptr };
		usize size{ 0 };
		friend class base::StrID;

	public:
		OwningView() = default;

		explicit OwningView(std::nullptr_t) {}

		/**
		 * @note Takes ownership, begin should be on heap.
		 */
		OwningView(byte* begin, usize size): begin{ begin }, size{ size } {}

		// Makes copy
		explicit OwningView(const char* const c_str) { *this = RawView(c_str).memoryCopy(); }

		OwningView static copy(RawView view) { return view.memoryCopy(); }

		OwningView(const OwningView&) = delete;

		OwningView(OwningView&& view) noexcept { *this = std::move(view); }

		OwningView& operator=(OwningView&& view) noexcept {
			begin      = view.begin;
			size       = view.size;
			view.begin = nullptr;
			view.size  = 0;
			return *this;
		}

		RawView view() { return { begin, size }; }

		[[nodiscard]]
		const RawView view() const {
			return { begin, size };
		}

		[[nodiscard]]
		const ModRawView modView() {
			return { begin, size };
		}

		~OwningView() { delete[] begin; }
	};

	/**
	 * @brief Shared immutable byte array view
	 */
	class SharedView final {
		std::shared_ptr<OwningView> content;

	public:
		SharedView(const SharedView&) = default;
		SharedView(SharedView&&)      = default;

		SharedView& operator=(const SharedView&) = default;
		SharedView& operator=(SharedView&&)      = default;

		/**
		 * @note Takes ownership of shared_ptr
		 */
		explicit SharedView(std::shared_ptr<OwningView> content): content(std::move(content)) {}

		/**
		 * @note Takes ownership, begin should be on heap.
		 */
		SharedView(byte* begin, usize size): content(std::make_shared<OwningView>(begin, size)) {}

		SharedView static copy(RawView view) {
			return SharedView(std::make_shared<OwningView>(OwningView::copy(view)));
		}

		// Makes copy
		explicit SharedView(const char* const c_str):
			  content(std::make_shared<OwningView>(c_str)) {}

		[[nodiscard]]
		const RawView view() const;
	};
}

// std::hash functor for RawView:
namespace std {
	template<>
	struct hash<base::RawView> final {
		usize operator()(const base::RawView& k) const {
			return std::hash<std::string_view>()(k.stringView());
		}
	};
}
