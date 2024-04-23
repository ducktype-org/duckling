#include <iostream>
#include <cstdlib>

#include "stream_printer.hpp"

namespace printer {
	// All background color escape codes are 10 above foregrounds colors.
	ColorId calculateBackgroundColorId(const ColorId color_id) {
		constexpr int8_t background_font_color_offset = 10;
		return static_cast<ColorId>(
			color_id > 0 ? color_id + background_font_color_offset : color_id
		);
	}

	StreamPrinter& StreamPrinter::setMinLevel(const MessageType type, const LevelType level) {
		if (type == MessageType::ALL)
			minLevel.fill(level);
		else
			minLevel.at(msgToInt(type)) = level;
		return *this;
	}

	StreamPrinter& StreamPrinter::setGeneralMax(const usize max) {
		generalMax = max;
		return *this;
	}

	StreamPrinter& StreamPrinter::setMaxAmounts(const MessageType type, const usize amount) {
		if (type == MessageType::ALL)
			maxAmounts.fill(amount);
		else
			maxAmounts.at(msgToInt(type)) = amount;
		return *this;
	}

	StreamPrinter& StreamPrinter::add(const MessagePack& pack) {
		messagePacks.push_back(pack);
		return *this;
	}

	StreamPrinter& StreamPrinter::add(MessagePack&& pack) {
		messagePacks.emplace_back(std::move(pack));
		return *this;
	}

	StreamPrinter& StreamPrinter::add(const Message& message) {
		messagePacks.push_back({ message });
		return *this;
	}

	StreamPrinter& StreamPrinter::add(Message&& message) {
		MessagePack pack;
		pack.emplace_back(std::move(message));
		messagePacks.push_back(std::move(pack));
		return *this;
	}

	void StreamPrinter::print(std::ostream& out) const {
		std::array<usize, TYPE_COUNT> currentAmounts = {};
		currentAmounts.fill(0);
		usize currentCount = 0;

		for (const MessagePack& pack: messagePacks) {
			for (const Message& message: pack) {
				if (const MessageTypeId messageType = msgToInt(message.type);
				    message.level >= minLevel.at(messageType)) {
					if (currentAmounts.at(messageType) > maxAmounts.at(messageType)) continue;

					if (currentCount == generalMax) {
						out << "Limit for messages has been reached.\n";
						return;
					}

					if (currentAmounts.at(messageType) == maxAmounts.at(messageType)) {
						out << "Limit for this type of message has been reached.\n";
					} else {
						for (const MessageContent& content: message.contents) {
							ColorId foreground_color_id{}, background_color_id{};
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
					currentAmounts.at(messageType)++;
				}
			}
		}
	}

	void StreamPrinter::clear() { messagePacks.resize(0); }
}
