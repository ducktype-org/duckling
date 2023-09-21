#include "../exceptions.hpp"

#include <ostream>

namespace base {

	Panic::Panic(std::string position, std::string reason): position(std::move(position)), reason(std::move(reason)) {
		this->reason += '\0';
		makeWhatStr();
	}

	void Panic::makeWhatStr() {
		what_str.clear();
		what_str += "Unexpected compiler error occurred:\n";
		what_str += getPosition() + ":\n";
		what_str += reason + ":\n";
	}

	const std::string& Panic::getPosition() const {
			return position;
	}

	const char* Panic::what() const noexcept {
		return what_str.c_str();
	}

	void Panic::print(std::ostream& out) const {
		// @TODO: use printer/error framework here
		out << what_str;
	}

	LogicError::LogicError(std::string message): message(std::move(message)) {
		this->message += '\0';
	}

	const char* LogicError::what() const noexcept {
		return message.data();
	}

	NotYetImplemented::NotYetImplemented(std::string message) {
		this->message = "The feature is not implemented yet.\n";
		this->message += message;
		this->message += '\0';
	}

	const char* NotYetImplemented::what() const noexcept {
		return message.data();
	}

}
