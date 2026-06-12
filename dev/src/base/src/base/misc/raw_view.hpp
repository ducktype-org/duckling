/**
 * @file raw_view.hpp
 * @brief Provides byte array views.
 */
#pragma once

#include <base/types/ints.hpp>

#include <memory>
#include <span>
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

		RawView(std::span<const std::byte> span): RawView(span.data(), span.size()) {}

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

	/**
	 * @brief Non owning mutable typed array view
	 */
	template<typename T>
	class TypedModRawView final {
	private:
		T*    begin    = nullptr;
		usize arr_size = 0;

	public:
		TypedModRawView()                       = default;
		TypedModRawView(const TypedModRawView&) = default;
		TypedModRawView(TypedModRawView&&)      = default;

		TypedModRawView(T* begin, usize size): begin{ begin }, arr_size{ size } {}

		TypedModRawView& operator=(const TypedModRawView&) = default;

		[[nodiscard]]
		usize size() const {
			return arr_size;
		}

		[[nodiscard]]
		T* getBegin() const {
			return begin;
		}

		[[nodiscard]]
		T& operator[](usize index) {
			return begin[index];
		}

		[[nodiscard]]
		const T& operator[](usize index) const {
			return begin[index];
		}

		[[nodiscard]]
		TypedModRawView subview(usize from, usize count) const {
			return { begin + from, count };
		}

		[[nodiscard]]
		std::string_view stringView() const {
			return { reinterpret_cast<const char*>(begin), arr_size };
		}

		[[nodiscard]]
		std::string stdString() const {
			return std::string(stringView());
		}

		[[nodiscard]]
		ModRawView modRawView() const {
			return { reinterpret_cast<byte*>(begin), arr_size * sizeof(T) };
		}

		operator ModRawView() const requires std::same_as<std::remove_const_t<T>, std::byte> {
			return { begin, arr_size };
		}
	};

	/**
	 * @brief Owning typed array view
	 */
	template<typename T>
	class TypedOwningView final {
		T*    begin{ nullptr };
		usize arr_size{ 0 };

	public:
		TypedOwningView() = default;

		explicit TypedOwningView(std::nullptr_t) {}

		/**
		 * @note Takes ownership, begin should be on heap.
		 */
		TypedOwningView(T* begin, usize size): begin{ begin }, arr_size{ size } {}

		TypedOwningView(const TypedOwningView&) = delete;

		TypedOwningView(TypedOwningView&& view) noexcept { *this = std::move(view); }

		TypedOwningView& operator=(TypedOwningView&& view) noexcept {
			if (this != &view) {
				delete[] begin;
				begin         = view.begin;
				arr_size      = view.arr_size;
				view.begin    = nullptr;
				view.arr_size = 0;
			}
			return *this;
		}

		[[nodiscard]]
		T* getBegin() const {
			return begin;
		}

		[[nodiscard]]
		usize size() const {
			return arr_size;
		}

		[[nodiscard]]
		TypedModRawView<T> modView() {
			return { begin, arr_size };
		}

		[[nodiscard]]
		TypedModRawView<const T> view() const {
			return { begin, arr_size };
		}

		~TypedOwningView() { delete[] begin; }
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
			if (this != &view) {
				delete[] begin;
				begin      = view.begin;
				size       = view.size;
				view.begin = nullptr;
				view.size  = 0;
			}
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
