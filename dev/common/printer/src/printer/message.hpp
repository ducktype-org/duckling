#pragma once

#include <base/ints.hpp>
#include <iostream>
#include <utility>

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

	class StreamPrinter;

	class MessageContent {
		MessageContentText str;
		Color              foreground_color;
		Color              background_color;
		friend StreamPrinter;

	public:
		MessageContent()                                 = delete;
		MessageContent(const MessageContent&)            = default;
		MessageContent(MessageContent&&)                 = default;
		MessageContent& operator=(const MessageContent&) = default;
		MessageContent& operator=(MessageContent&&)      = default;

		MessageContent(
			const char* str,
			const Color foreground_color = Color::DEFAULT,
			const Color background_color = Color::DEFAULT
		):
			  str(str),
			  foreground_color(foreground_color),
			  background_color(background_color) {}

		MessageContent(
			MessageContentText str,
			const Color        foreground_color = Color::DEFAULT,
			const Color        background_color = Color::DEFAULT
		):
			  str(std::move(str)),
			  foreground_color(foreground_color),
			  background_color(background_color) {}
	};

	// Not to be confused with diagnostic::Message
	class Message {
		std::vector<MessageContent> contents;
		MessageType                 type;
		LevelType                   level;
		Color                       foreground_color;
		Color                       background_color;
		friend StreamPrinter;

	public:
		Message()               = delete;
		Message(const Message&) = default;
		Message(Message&& oth)  = default;

		Message(
			std::vector<MessageContent> list,
			const MessageType           type             = MessageType::GENERAL,
			const LevelType             level            = 0,
			const Color                 foreground_color = Color::RESET,
			const Color                 background_color = Color::RESET
		):
			  contents(std::move(list)),
			  type(type),
			  level(level),
			  foreground_color(foreground_color),
			  background_color(background_color) {}

		void add(const MessageContent& mc) { contents.push_back(mc); }

		void add(std::vector<MessageContent> mc) {
			contents.insert(
				contents.end(),
				std::make_move_iterator(mc.begin()),
				std::make_move_iterator(mc.end())
			);
		}
	};

	// @FIXME MessagePack and Message(init_list) constructors can be ambiguous
	typedef std::vector<Message> MessagePack;
}
