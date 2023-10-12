/**
 * @file printer.hpp
 * @brief
 * Module for outputting messages to console.
 *
 * Basic usage: playground/printer_test.hpp
 */

#pragma once

#include <base/ints.hpp>
#include <string>
#include <initializer_list>
#include <vector>
#include <array>
#include <limits.h>
#include <iostream>

namespace printer {
	// Important to update this value, when adding or removing MessageTypes.
	// @IDEA: Make TYPE_COUNT the last member of enum class MessageType, and assign its value there.
	// This way, it will be harder to forget to update it whenever making changes to MessageType.
	constexpr usize TYPE_COUNT = 6;

	typedef i32 LevelType;

	typedef std::string MessageContentText;

	typedef int8_t MessageTypeId;

	// Aside from 'ALL', types have to be enumerated from 0 to TYPE_COUNT - 1.
	enum class MessageType : MessageTypeId {
		ALL     = -1,  // Used to change Console options for all MessageTypes.
		ERROR   = 0,
		DEBUG   = 1,
		NOTE    = 2,
		HINT    = 3,
		WARNING = 4,
		GENERAL = 5,
		// Important to update TYPE_COUNT value, when adding or removing MessageTypes.
	};

	constexpr MessageTypeId msgToInt(MessageType msg) { return static_cast<MessageTypeId>(msg); }

	// @TODO: Determine the correct type for enum class Color.
	typedef int8_t ColorId;
	// @IDEA: Add possibility and functionality for custom colors. Reference ANSI escape code 38.
	/**
	 * Important to note that these colors are inconsistent across terminals and can have
	 * deceiving names. For example WHITE is grayish (BRIGHT_WHITE is closer to real white)
	 * and YELLOW is white in Windows PowerShell. Names were taken from Wikipedia. I recommend
	 * taking a look to consult which colors you should use and how they will be displayed.
	 * https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
	 *
	 * Positive integers indicate foreground ANSI escape code ids. Background ids are achieved
	 * by adding 10 to foreground ids.
	 *
	 * DEFAULT is for MessageContent, it uses the message's default color. If message color is not
	 * set, it's the defaultMessageColor from printer.cpp. Do not set message's color to DEFAULT.
	 * RESET resets color settings to terminal's default.
	 */
	enum class Color : ColorId {
		DEFAULT = -1,
		RESET   = 0,

		BLACK          = 30,
		RED            = 31,
		GREEN          = 32,
		YELLOW         = 33,
		BLUE           = 34,
		MAGENTA        = 35,
		CYAN           = 36,
		WHITE          = 37,
		GRAY           = 90,
		BRIGHT_RED     = 91,
		BRIGHT_GREEN   = 92,
		BRIGHT_YELLOW  = 93,
		BRIGHT_BLUE    = 94,
		BRIGHT_MAGENTA = 95,
		BRIGHT_CYAN    = 96,
		BRIGHT_WHITE   = 97,
	};

	class Console;

	class MessageContent {
	private:
		MessageContentText str;
		Color              foreground_color;
		Color              background_color;
		friend Console;

	public:
		MessageContent()                                 = delete;
		MessageContent(const MessageContent&)            = default;
		MessageContent(MessageContent&&)                 = default;
		MessageContent& operator=(const MessageContent&) = default;
		MessageContent& operator=(MessageContent&&)      = default;

		MessageContent(
			const char* str,
			Color       foreground_color = Color::DEFAULT,
			Color       background_color = Color::DEFAULT
		):
			  str(str),
			  foreground_color(foreground_color),
			  background_color(background_color) {}

		MessageContent(
			MessageContentText str,
			Color              foreground_color = Color::DEFAULT,
			Color              background_color = Color::DEFAULT
		):
			  str(str),
			  foreground_color(foreground_color),
			  background_color(background_color) {}
	};

	class Message {
	private:
		std::vector<MessageContent> contents;
		MessageType                 type;
		LevelType                   level;
		Color                       foreground_color;
		Color                       background_color;
		friend Console;

		Message() = delete;

	public:
		Message(const Message&) = default;
		Message(Message&& oth)  = default;

		Message(
			std::vector<MessageContent> list,
			MessageType                 type             = MessageType::GENERAL,
			LevelType                   level            = 0,
			Color                       foreground_color = Color::RESET,
			Color                       background_color = Color::RESET
		):
			  contents(std::move(list)),
			  type(type),
			  level(level),
			  foreground_color(foreground_color),
			  background_color(background_color) {}

		void add(const MessageContent&);
		void add(std::vector<MessageContent>);

		void print(std::ostream& out = std::cerr);
	};

	// @FIXME MessagePack and Message(init_list) constructors can be ambiguous
	typedef std::vector<Message> MessagePack;

	typedef std::array<LevelType, TYPE_COUNT> minLevel_t;
	typedef std::array<usize, TYPE_COUNT>     maxAmounts_t;

	namespace detail {
		static constexpr minLevel_t constructDefaultMinLevel() {
			minLevel_t res = {};
			res.fill(0);
			return res;
		}

		static constexpr maxAmounts_t constructDefaultMaxAmounts() {
			maxAmounts_t res = {};
			res.fill(SIZE_MAX);
			return res;
		}
	}

	class Console {
	private:
		static constexpr minLevel_t   defaultMinLevel   = detail::constructDefaultMinLevel();
		static constexpr maxAmounts_t defaultMaxAmounts = detail::constructDefaultMaxAmounts();

		std::vector<MessagePack> messagePacks;
		usize                    generalMax;
		minLevel_t               minLevel;
		maxAmounts_t             maxAmounts;

	public:
		Console(
			usize        generalMax = SIZE_MAX,
			minLevel_t   minLevel   = defaultMinLevel,
			maxAmounts_t maxAmounts = defaultMaxAmounts
		):
			  generalMax(generalMax),
			  minLevel(minLevel),
			  maxAmounts(maxAmounts) {}

		// @IDEA: make these sets constexpr (and implement them as such).
		void setMinLevel(MessageType type, LevelType level);

		void setGeneralMax(usize max);

		void setMaxAmounts(MessageType type, usize amount);

		void add(const MessagePack& pack);
		void add(MessagePack&& pack);
		void add(const Message& message);
		void add(Message&& message);

		void print(std::ostream& out = std::cerr) const;

		void clear();
	};
}
