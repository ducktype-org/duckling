#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <string>

namespace base {
	typedef const uint8_t* RawArray;


	/**
	 * @brief Non owning byte array view
	 *
	 */
	class RawView {
	private:
		friend class OwningView;

		[[nodiscard]] RawView memoryCopy() const;

		RawArray begin = nullptr;
		size_t arr_size = 0;

	public:
		RawView() = default;
		RawView(const RawView&) = default;
		RawView(RawView&&) = default;
		RawView(RawArray begin, std::size_t size): begin{begin}, arr_size{size} {}

		RawView& operator=(const RawView&) = default;

		// do not make explicit
		RawView(const char* c_str);

		[[nodiscard]] std::string_view stringView() const;
		[[nodiscard]] std::string stdString() const;
		[[nodiscard]] RawView subSuffix(std::size_t from) const;

		[[nodiscard]] std::size_t size() const;
		[[nodiscard]] RawArray getBegin() const;

		uint8_t operator[](std::size_t index);
		bool operator==(const RawView& oth) const;
	};

	/**
	 * Non owning byte array view
	*/
	class ModRawView {
	private:
		uint8_t* begin = nullptr;
		size_t arr_size = 0;

	public:
		ModRawView() = default;
		ModRawView(const ModRawView&) = default;
		ModRawView(ModRawView&&) = default;
		ModRawView(uint8_t* begin, std::size_t size): begin{begin}, arr_size{size} {}

		ModRawView& operator=(const ModRawView&) = default;

		[[nodiscard]] std::size_t size() const { return arr_size; };
		[[nodiscard]] uint8_t* getBegin() const { return begin; }
	};

	class StrId;

	/**
	 * Owning byte array view
	*/
	class OwningView {
		uint8_t* begin{};
		size_t size{};
		friend class base::StrId;

	public:
		OwningView(): begin(nullptr), size{0} {}
		explicit OwningView(std::nullptr_t): begin(nullptr), size{0} {}

		// Takes ownership, begin should be on heap
		OwningView(uint8_t* begin, size_t size): begin{begin}, size{size} {}

		// Makes copy
		explicit OwningView(const char* const c_str) {
			auto aux = RawView(c_str).memoryCopy();
			begin = const_cast<uint8_t*>(aux.begin);
			size = aux.arr_size;
		}

		OwningView static copy(RawView view) {
			OwningView out;
			auto aux = view.memoryCopy();
			out.begin = const_cast<uint8_t*>(aux.begin);
			out.size = aux.arr_size;
			return out;
		}

		OwningView(const OwningView&) = delete;
		OwningView(OwningView&& view) noexcept {
			*this = std::move(view);
		};

		OwningView& operator=(OwningView&& view) noexcept {
			begin = view.begin;
			size = view.size;
			view.begin = nullptr;
			view.size = 0;
			return *this;
		};

		RawView view() { return {begin, size}; }

		~OwningView() {
			delete[] begin;
		}
	};
}

// std::hash functor for RawView:
namespace std {
	template<>
	struct hash<base::RawView> {
		std::size_t operator()(const base::RawView& k) const {
			return std::hash<std::string_view>()(k.stringView());
		}
	};
}
