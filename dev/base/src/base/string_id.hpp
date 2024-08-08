/**
 * @file string_id.hpp
 */

#pragma once

#include "strongly_typed_id.hpp"
#include "maps.hpp"
#include "raw_view.hpp"
#include <string>
#include <charconv>

namespace base {

	namespace detail {
		STRONG_TYPEDEF_ID(StrInnerID);
	}

	class StrId {
		using InnerId = detail::StrInnerID;
		InnerId id;

		using ToDataType = VectorMap<InnerId, RawView>;
		using ToIdType   = HashMap<RawView, InnerId>;

		static ToDataType to_data_map;
		static ToIdType   to_id_map;

	public:
		StrId(): id(InnerId::bad()){};
		StrId(const StrId& oth) = default;
		StrId(StrId&& oth)      = default;

		explicit StrId(char character);

		// Makes copy
		explicit StrId(const RawView& data);
		explicit StrId(const char* data);

		StrId& operator=(const StrId& oth) = default;

		[[nodiscard]]
		base::RawView view() const {
			DUCKLING_ASSERT(id.isGood(), "StrId is bad");
			return to_data_map[id];
		}

		[[nodiscard]]
		std::string_view strView() const {
			return view().stringView();
		}

		[[nodiscard]]
		std::string str() const {
			return view().stdString();
		}

		[[nodiscard]]
		bool isBad() const {
			return id.isBad();
		}

		[[nodiscard]]
		bool isGood() const {
			return !id.isBad();
		}

		bool operator==(const StrId& oth) const { return id == oth.id; }

		bool operator==(RawView oth) const { return view().stringView() == oth.stringView(); }

		bool operator!=(const StrId& oth) const { return id != oth.id; }

		bool operator<(const StrId& oth) const { return id < oth.id; }

		explicit operator usize() const { return usize(id); }

		friend void swap(StrId& first, StrId& second) {
			using std::swap;
			swap(first.id, second.id);
		}

		/**
		 * For debug purposes
		 * Prints content of StrId inner data
		 */
		static void dumpData(std::ostream& out);

		/**
		 * For debug purposes
		 * Get internal ID
		 */
		[[nodiscard]]
		InnerId innerID() const {
			return this->id;
		}

		friend class std::hash<StrId>;
	};

	/**
	 * @brief Converts a string to the value of the number it contains.
	 *
	 * Raises exception on error.
	 */
	i64 strIdToNum(base::StrId str);

	namespace detail {
		inline void strConcat(std::string& out, StrId str_id) { out.append(str_id.strView()); }
	}
}

namespace std {
	template<>
	struct hash<base::StrId> {
		usize operator()(const base::StrId& x) const { return x.id.asInt(); }
	};
}
