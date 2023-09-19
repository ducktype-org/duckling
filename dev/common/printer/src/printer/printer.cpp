/**
 * @file printer.cpp
 */

#include "printer.hpp"

#include <cstdlib>
#include <iostream>

namespace printer {
	// All background color escape codes are 10 above foregrounds colors.
	ColorId calculateBackgroundColorId(ColorId color_id) {
		constexpr int8_t background_font_color_offset = 10;
		return color_id > 0 ? color_id + background_font_color_offset : color_id;
	}

	void Message::add(const MessageContent& mc) {
		this->contents.push_back(mc);
	}

	void Message::add(std::vector<MessageContent> mc) {
		this->contents.insert(this->contents.end(),
		                      std::make_move_iterator(mc.begin()),
		                      std::make_move_iterator(mc.end()));
	}

	void Message::print(std::ostream& out) {
		Console console;
		console.add(*this);
		console.print(out);
	}

	void Console::setMinLevel(MessageType type, LevelType level) {
		if (type == MessageType::ALL)
			minLevel.fill(level);
		else
			minLevel[printer::msgToInt(type)] = level;
	}

	void Console::setGeneralMax(usize max) {
		generalMax = max;
	}

	void Console::setMaxAmounts(MessageType type, usize amount) {
		if (type == MessageType::ALL)
			maxAmounts.fill(amount);
		else
			maxAmounts[printer::msgToInt(type)] = amount;
	}

	void Console::add(const MessagePack& pack) {
		messagePacks.push_back(std::move(pack));
	}

	void Console::add(MessagePack&& pack) {
		messagePacks.emplace_back(std::move(pack));
	}

	void Console::add(const Message& message) {
		messagePacks.push_back({ message });
	}

	void Console::add(Message&& message) {
		MessagePack pack;
		pack.emplace_back(std::move(message));
		messagePacks.push_back(std::move(pack));
	}

	void Console::print(std::ostream& out) const {
		std::array<usize, TYPE_COUNT> currentAmounts = {};
		currentAmounts.fill(0);
		usize currentCount = 0;

		for (const MessagePack& pack : messagePacks) {
			for (const Message& message : pack) {
				MessageTypeId messageType = msgToInt(message.type);
				if (message.level >= minLevel[messageType]) {
					if (currentAmounts[messageType] > maxAmounts[messageType]) continue;

					if (currentCount == generalMax) {
						out << "Limit for messages has been reached.\n";
						return;
					}

					if (currentAmounts[messageType] == maxAmounts[messageType]) {
						out << "Limit for this type of message has been reached.\n";
					} else {
						for (const MessageContent& content : message.contents) {
							ColorId foreground_color_id, background_color_id;
							if (content.foreground_color == Color::DEFAULT) {
								foreground_color_id
									= static_cast<ColorId>(message.foreground_color);
							} else {
								foreground_color_id
									= static_cast<ColorId>(content.foreground_color);
							}
							if (content.background_color == Color::DEFAULT) {
								background_color_id
									= static_cast<ColorId>(message.background_color);
							} else {
								background_color_id
									= static_cast<ColorId>(content.background_color);
							}
							// Background colors have different ids than foreground colors.
							background_color_id = calculateBackgroundColorId(background_color_id);

							// Check for RESET first, because it resets both foreground and
							// background. This way, background RESET does not reset foreground
							// color that was just set.
							if (background_color_id == static_cast<ColorId>(Color::RESET)
							    || foreground_color_id == static_cast<ColorId>(Color::RESET)) {
								out << "\033[0m";
							}

							if (foreground_color_id > 0)
								out << "\033[" + std::to_string(foreground_color_id) + "m";
							if (background_color_id > 0)
								out << "\033[" + std::to_string(background_color_id) + "m";

							out << content.str;
						}
						// Reset color settings after message ends.
						out << "\033[0m\n";
						currentCount++;
					}
					currentAmounts[messageType]++;
				}
			}
		}
	}

	void Console::clear() {
		messagePacks.resize(0);
	}
}
