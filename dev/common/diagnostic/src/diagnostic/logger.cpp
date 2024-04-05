#include "logger.hpp"
#include <filesystem/file.hpp>

namespace dia {
	void Logger::log(base::unique_ptr<Message> message_ptr) {
		const int severity_id = static_cast<int>(message_ptr->getSeverity());
		message_log[severity_id].emplace_back(message_ptr.release());
	}

	static void dumpMessages(
		const std::vector<base::unique_ptr<Message>>& messages,
		printer::Console&                             console,
		const bool                                    detailed
	) {
		for (const auto& message_ptr: messages)
			console.add(message_ptr->toPrinterMessagePack(detailed));
	}

	void Logger::dumpLog(const bool detailed, std::ostream& stream) const {
		auto console = printer::Console{};
		for (int severity_id = 0; severity_id < Message::NUM_SEVERITIES; severity_id++)
			dumpMessages(message_log[severity_id], console, detailed);
		console.print(stream);
	}

	usize Logger::messageCount(Message::Severity s) const {
		return message_log[static_cast<int>(s)].size();
	}

	usize Logger::messageCount() const {
		return messageCount(Message::Severity::Error) + messageCount(Message::Severity::Warning)
		     + messageCount(Message::Severity::Info);
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
		printer::MessageContent toMessageContentBrief() const override {
			return printer::MessageContent(message);
		}

		Domain getDomain() const override { return Domain::Misc; }
	};

	void Logger::failAndLog(const SourcePosition& position, std::string_view message) {
		log(base::make_unique<ObsoleteErrorWithPositionAndString>(position, message));
	}

	class ObsoleteErrorWithPrinterMessage final: public Error {
	public:
		explicit ObsoleteErrorWithPrinterMessage(const printer::MessageContent& message):
			  // Had to pick a file that always exists and behaves somewhat normally.
			  // /dev/zero does not work.
			  Error(
				  { std::make_shared<fs::FilePath>(std::filesystem::path("/usr/bin/cat")), 1, 1, 1 }
			  ),
			  message(message) {}

	private:
		printer::MessageContent message;

	protected:
		printer::MessageContent toMessageContentBrief() const override {
			return message;
		}

		Domain getDomain() const override { return Domain::Misc; }
	};

	void Logger::failAndLog(const printer::MessageContent& message) {
		log(base::make_unique<ObsoleteErrorWithPrinterMessage>(message));
	}
}
