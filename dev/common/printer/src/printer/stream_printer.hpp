/**
 * @file stream_printer.hpp
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

	class StreamPrinter {
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

		[[nodiscard]]
		bool isIgnoreInstantDebug() const {
			return ignore_instant_debug;
		}

		StreamPrinter(
			const usize         generalMax = SIZE_MAX,
			const minLevel_t&   minLevel   = defaultMinLevel,
			const maxAmounts_t& maxAmounts = defaultMaxAmounts
		):
			  generalMax(generalMax),
			  minLevel(minLevel),
			  maxAmounts(maxAmounts) {}

		// @IDEA: make these sets constexpr (and implement them as such).
		StreamPrinter& setMinLevel(MessageType type, LevelType level);

		StreamPrinter& setGeneralMax(usize max);

		StreamPrinter& setMaxAmounts(MessageType type, usize amount);

		StreamPrinter& add(const MessagePack& pack);
		StreamPrinter& add(MessagePack&& pack);
		StreamPrinter& add(const Message& message);
		StreamPrinter& add(Message&& message);

		void print(std::ostream& out = std::cerr) const;

		void clear();
	};
}
