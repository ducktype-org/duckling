/**
 * @file printer_console.hpp
 * @brief Module for outputting messages to console.
 *
 * Basic usage: playground/printer_test.hpp
 */

#pragma once

#include <base/ints.hpp>
#include "message.hpp"

#include <vector>
#include <array>
#include <climits>
#include <iostream>

namespace printer {
	using minLevel_t   = std::array<LevelType, TYPE_COUNT>;
	using maxAmounts_t = std::array<usize, TYPE_COUNT>;

	class Console {
		static constexpr minLevel_t defaultMinLevel = [] {
			minLevel_t res = {};
			res.fill(0);
			return res;
		}();
		static constexpr maxAmounts_t defaultMaxAmounts = [] {
			maxAmounts_t res = {};
			res.fill(SIZE_MAX);
			return res;
		}();

		std::vector<MessagePack> messagePacks;
		usize                    generalMax;
		minLevel_t               minLevel;
		maxAmounts_t             maxAmounts;
		bool                     ignore_instant_debug = false;

	public:
		void ignoreInstantDebug() { ignore_instant_debug = true; }

		bool isIgnoreInstantDebug() const { return ignore_instant_debug; }

		Console(
			const usize         generalMax = SIZE_MAX,
			const minLevel_t&   minLevel   = defaultMinLevel,
			const maxAmounts_t& maxAmounts = defaultMaxAmounts
		):
			  generalMax(generalMax),
			  minLevel(minLevel),
			  maxAmounts(maxAmounts) {}

		// @IDEA: make these sets constexpr (and implement them as such).
		Console& setMinLevel(MessageType type, LevelType level);

		Console& setGeneralMax(usize max);

		Console& setMaxAmounts(MessageType type, usize amount);

		Console& add(const MessagePack& pack);
		Console& add(MessagePack&& pack);
		Console& add(const Message& message);
		Console& add(Message&& message);

		void print(std::ostream& out = std::cerr) const;

		void clear();
	};
}
