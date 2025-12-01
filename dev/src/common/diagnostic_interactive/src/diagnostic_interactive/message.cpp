#include "message.hpp"

#include "diagnostic_interactive/core/diagnostic_file.hpp"

#include "base/collections/maps.hpp"

#include "diagnostic/source_position.hpp"

#include <string>

namespace dia_int {

	Metadata::operator dia_file::Metadata() const {
		return dia_file::Metadata{
			.template_type = template_type, .type = type, .family = family, .name = name
		};
	}

	Box<dia_file::Component> TextArgument::getValue(MessageBase&) {
		return base::makeBox<dia_file::TextComponent>(content);
	}

	Box<dia_file::Component> CodeLocationArgument::getValue(MessageBase&) {
		return base::makeBox<dia_file::CodeLocationComponent>(
			location.file, location.line, location.column
		);
	}

	void CodeArgument::addCodeLines(
		std::vector<Box<dia_file::Component>>& code_list,
		Ref<tokenizer::TokenSource>            source,
		usize                                  start,
		usize                                  end
	) {
		auto lines = source->viewSplitRange(start, end);
		if (lines.empty()) return;

		if (source->getLineColumn(start).second
		    == 1)  // First  character in line, we need a start line.
			code_list.emplace_back(
				makeBox<dia_file::StartLineComponent>(source->getLineColumn(start).first)
			);
		code_list.emplace_back(makeBox<dia_file::CodeComponent>(lines[0].second.stdString()));

		for (usize i = 1; i < lines.size(); i++) {
			code_list.emplace_back(makeBox<dia_file::StartLineComponent>(lines[i].first));
			code_list.emplace_back(makeBox<dia_file::CodeComponent>(lines[i].second.stdString()));
		}
	}

	std::vector<PointerMessage> filterMessages(
		const std::vector<PointerMessage>& messages, dia::SourcePosition snippet_position
	) {
		std::vector<PointerMessage> result;
		for (const auto& msg: messages) {
			if (msg.position.getStart() >= snippet_position.getStart()
			    && msg.position.getEnd() <= snippet_position.getEnd()
			    && msg.position.getLocation()->getSourceFile()
			           == snippet_position.getLocation()->getSourceFile()) {
				result.push_back(msg);
			}
		}
		return result;
	}

	Box<dia_file::Component> CodeArgument::getValue(MessageBase& diag) {
		// Here would be a lot of code to extract the code fragment from the source file.
		// And potentially add interactive contents.
		auto source = position.getSource();

		usize start_line = position.getStartLineColumn().first;
		usize end_line   = position.getEndLineColumn().first;

		usize first_line = std::max(1 + lines_before, start_line) - lines_before;
		usize last_line  = std::min(source->getLines().size(), end_line + lines_after);

		usize begin_char = source->getLine(first_line).first;
		usize end_char   = source->getLine(last_line).second + 1;  // excluding last line


		auto code_list = std::vector<Box<dia_file::Component>>();

		const auto& pointer_messages = filterMessages(
			diag.getPointerMessages(),
			dia::SourcePosition(position.getLocation(), begin_char, end_char - 1)
		);
		if (pointer_messages.empty()) {
			addCodeLines(code_list, source, begin_char, end_char);

			return base::makeBox<dia_file::ConcatComponent>(std::move(code_list));
		} else if (pointer_messages.size() == 1) {
			// Single pointer message - highlight the code fragment.
			auto& pointer_msg = pointer_messages[0];

			usize pointer_start = pointer_msg.position.getStart();
			usize pointer_end   = pointer_msg.position.getEnd() + 1;

			// Add code before the pointer.
			addCodeLines(code_list, source, begin_char, pointer_start);

			// Add highlighted code.
			auto highlighted_code_list = std::vector<Box<dia_file::Component>>();

			addCodeLines(highlighted_code_list, source, pointer_start, pointer_end);

			code_list.emplace_back(base::makeBox<dia_file::PointedComponent>(
				makeBox<dia_file::ConcatComponent>(std::move(highlighted_code_list)),
				std::vector<dia_file::PointerMessage>{ dia_file::PointerMessage{ pointer_msg.name } }
			));

			// Add code after the pointer.
			addCodeLines(code_list, source, pointer_end, end_char);

			return base::makeBox<dia_file::ConcatComponent>(std::move(code_list));
		} else {
			CORE_PANIC("Not implemented: multiple pointer messages in CodeArgument");
			// Multiple pointer messages - for simplicity, just return the code without highlights.
			addCodeLines(code_list, source, begin_char, end_char);
			return base::makeBox<dia_file::ConcatComponent>(std::move(code_list));
		}
	}

	dia_file::Message MessageBase::buildMessages(
		base::HashMap<std::string, dia_file::Message>& additional_messages
	) {
		dia_file::Message msg;
		msg.metadata = getMetadata();
		for (const auto& arg: arguments) msg.arguments.put(arg->getName(), arg->getValue(*this));


		msg.attached_messages.reserve(this->attached_messages.size());

		for (const auto& attached_msg: attached_messages) {
			auto id = MessageBase::getUniqueID();
			msg.attached_messages.push_back(id);
			additional_messages.put(id, attached_msg->buildMessages(additional_messages));
		}

		for (const auto& [id, value]: this->linked_messages)
			additional_messages.put(id, value->buildMessages(additional_messages));

		return msg;
	}

	Box<dia_int::dia_file::Thread> MessageBase::buildDiagnosticFile() {
		Box<dia_file::Thread>                         thread = makeBox<dia_file::Thread>();
		base::HashMap<std::string, dia_file::Message> additional_messages;

		thread->main_message        = buildMessages(additional_messages);
		thread->additional_messages = std::move(additional_messages);

		return thread;
	}

	std::string MessageBase::getUniqueID() {
		static usize counter = 0;
		return base::strConcat("msg_", counter++);
	}

	dia_file::ExploreEdge ExploreLink::getValue(MessageBase& message) const {
		dia_file::ExploreEdge edge;
		for (const auto& arg: arguments) edge.params.put(arg->getName(), arg->getValue(message));
		edge.name = message_id;
		return edge;
	}
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(dia_int::MessageBase);
