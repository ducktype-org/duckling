#include "logger.hpp"
#include <filesystem/file.hpp>

namespace dia {
	void Logger::log(base::unique_ptr<Message> message_ptr) {
		const int severity_id = static_cast<int>(message_ptr->getSeverity());
		message_log.at(severity_id).emplace_back(message_ptr.release());
	}

	static void dumpMessages(
		const std::vector<base::unique_ptr<Message>>& messages,
		printer::StreamPrinter&                       stream_printer,
		const bool                                    detailed
	) {
		for (const auto& message_ptr: messages)
			stream_printer.add(message_ptr->toPrinterMessagePack(detailed));
	}

	void Logger::dumpLog(const bool detailed, std::ostream& stream) const {
		auto stream_printer = printer::StreamPrinter{};
		// Currently, errors are dumped first, then warnings, then infos.
		// It is not determined whether this is how we want it to stay.
		// This is a temporary, "good enough" solution.
		// Perhaps we will change it to showing all messages in order of appearance
		// in the source code, or maybe we will choose a completely separate strategy.
		// @TODO: resolve the above.
		for (int severity_id = 0; severity_id < Message::NUM_SEVERITIES; severity_id++)
			dumpMessages(message_log.at(severity_id), stream_printer, detailed);
		stream_printer.print(stream);
	}

	usize Logger::messageCount(Message::Severity s) const {
		return message_log.at(static_cast<int>(s)).size();
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
		printer::MessageContent toMessageContentBrief() const override {
			return { message };
		}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Misc;
		}
	};

	void Logger::failAndLog(const SourcePosition& position, std::string_view message) {
		log(base::make_unique<ObsoleteErrorWithPositionAndString>(position, message));
	}

	class ObsoleteErrorWithPrinterMessage final: public Error {
	public:
		explicit ObsoleteErrorWithPrinterMessage(printer::MessageContent message):
			  // Had to pick a file that always exists and behaves somewhat normally.
		      // /dev/zero does not work.
			  // @TODO
			  Error(
				  { nullptr, 1, 1, 1 }
			  ),
			  message(std::move(message)) {}

	private:
		printer::MessageContent message;

	protected:
		[[nodiscard]]
		printer::MessageContent toMessageContentBrief() const override {
			return message;
		}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Misc;
		}
	};

	void Logger::failAndLog(const printer::MessageContent& message) {
		log(base::make_unique<ObsoleteErrorWithPrinterMessage>(message));
	}
}
