/**
 * @file string_id.hpp
 */

#pragma once

#include "maps.hpp"
#include "named_id.hpp"
#include "raw_view.hpp"
#include <string>

namespace base {

	class StrId {
		typedef base::NamedId<base::RawView> InnerId;
		InnerId                              id;

		typedef base::VectorMap<InnerId, base::RawView> ToDataType;
		typedef base::HashMap<base::RawView, InnerId>   ToIdType;

		static ToDataType to_data_map;
		static ToIdType   to_id_map;

	public:
		StrId(): id(InnerId::bad()){};
		StrId(const StrId& oth) = default;
		StrId(StrId&& oth)      = default;

		explicit StrId(char character);

		// Makes copy
		explicit StrId(const base::RawView& data);
		explicit StrId(const char* data);

		void operator=(const StrId& oth) { id = oth.id; }

		[[nodiscard]]
		base::RawView view() const {
			RIFT_ASSERT(id.isGood(), "StrId is bad");
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

		bool operator==(base::RawView oth) const { return view().stringView() == oth.stringView(); }

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

		friend class std::hash<base::StrId>;
	};

	namespace detail {
		inline void strConcat(std::string& out, base::StrId str_id) {
			out.append(str_id.strView());
		}
	}
}

namespace std {
	template<>
	struct hash<base::StrId> {
		usize operator()(const base::StrId& x) const { return x.id.asInt(); }
	};
}
