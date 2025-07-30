#include "logger.hpp"

#include "diagnostic_converters.hpp"

#include <base/int_conv.hpp>

#include <ranges>

namespace dia {

	constinit bool Logger::immediately_dump = false;

	void Logger::setImmediatelyDump(bool value) { immediately_dump = value; }

	void Logger::log(Box<Message> message_ptr, const bool detailed, const bool immediately_dump_arg) {
		if (immediately_dump_arg) {
			printer::StreamPrinter::print(
				DiagnosticToUserConverter::toPrinterContents(message_ptr.ref(), detailed)
			);
			printer::StreamPrinter::newline(2);
		}

		const auto severity_id
			= base::safeIntConv<usize>(std::to_underlying(message_ptr->getSeverity()));
		message_log.at(severity_id).emplace_back(std::move(message_ptr));
	}

	template<DiagnosticToPrinterConverter Converter>
	void Logger::dumpLog(const bool detailed, std::ostream& stream) const {
		// Currently, errors are dumped first, then warnings, then infos.
		// It is not determined whether this is how we want it to stay.
		// This is a temporary, "good enough" solution.
		// Perhaps we will change it to showing all messages in order of appearance
		// in the source code, or maybe we will choose a completely separate strategy.
		// @TODO: resolve the above.
		constexpr auto borrower
			= [](const Box<Message>& message) -> CRef<Message> { return message.ref(); };

		auto messages = std::ranges::join_view(message_log);
		auto refed    = std::ranges::transform_view(messages, borrower);

		printer::StreamPrinter::print(Converter::listToPrinterContents(refed, detailed), stream);
	}

	template void Logger::dumpLog<DiagnosticToUserConverter>(bool, std::ostream&) const;

	template void Logger::dumpLog<DiagnosticToJSONConverter>(bool, std::ostream&) const;

	usize Logger::messageCount(Message::Severity s) const {
		return message_log.at(base::safeIntConv<usize>(std::to_underlying(s))).size();
	}

	usize Logger::messageCount() const {
		usize result = 0;
		for (int severity_id = 0; severity_id < Message::NUM_SEVERITIES; severity_id++)
			result += messageCount(static_cast<Message::Severity>(severity_id));
		return result;
	}

	// Behold, for what you see ahead is the land of the obsolete!

	class ObsoleteErrorWithPositionAndString final: public Error {
		std::string message;

	public:
		explicit ObsoleteErrorWithPositionAndString(
			const SourcePosition& source_position, const std::string_view message
		):
			  Error(source_position),
			  message(message) {}

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return message;
		}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Misc;
		}
	};

	void Logger::failAndLog(const SourcePosition& position, std::string_view message) {
		log(makeBox<ObsoleteErrorWithPositionAndString>(position, message));
	}

	class ObsoleteErrorWithString final: public Error {
	public:
		explicit ObsoleteErrorWithString(std::string message):
			  Error(dia::SourcePosition::fakePosition()),
			  message(std::move(message)) {}

	private:
		std::string message;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return message;
		}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Misc;
		}
	};

	void Logger::failAndLog(const std::string& message) {
		log(makeBox<ObsoleteErrorWithString>(message));
	}

	void Logger::clear() {
		for (auto& log_for_severity: message_log) log_for_severity.clear();
	}

	void Logger::dumpLogAndClear(const bool detailed, std::ostream& stream) {
		dumpLog(detailed, stream);
		clear();
	}
}
