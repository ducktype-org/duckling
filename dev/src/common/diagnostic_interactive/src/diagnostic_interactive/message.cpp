#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/message.hpp>


#include <base/collections/maps.hpp>

#include <diagnostic/source_position.hpp>

#include <algorithm>
#include <map>
#include <string>

namespace dia_int {

	Metadata::operator dia_args::Metadata() const {
		return dia_args::Metadata{
			.template_type = template_type, .type = type, .family = family, .name = name
		};
	}

	Box<dia_args::Component> TextArgument::getValue(MessageBase&) {
		return base::makeBox<dia_args::TextComponent>(content);
	}

	Box<dia_args::Component> CodeLocationArgument::getValue(MessageBase&) {
		return base::makeBox<dia_args::CodeLocationComponent>(
			location.file, location.line, location.column, location.end_line, location.end_column
		);
	}

	void CodeArgument::addCodeLines(
		std::vector<Box<dia_args::Component>>& code_list,
		Ref<tokenizer::TokenSource>            source,
		usize                                  start,
		usize                                  end
	) {
		auto lines = source->viewSplitRange(start, end);
		if (lines.empty()) return;

		auto char_range = source->getCharRange(start, start + 1).stringView();

		// This is a bit of a hack, we don't want empty lines to be empty because the highlighting
		// doesn't work for empty lines. This hack should replace ONLY empty lines with spaces with
		// the current usage but might need to be improved.
		constexpr auto NORMALIZE = [](const std::string& s) { return s == "" ? " " : s; };

		// The start can be at the first character of the line
		// but also at the last character of the previous line (like '\n').
		// This is because the source positions are inclusive and offsets count the '\n' characters,
		// so the `pos_end + 1` points
		// to the '\n' char of the same line instead of the first character of the next line.
		if (char_range[0] == '\n' or char_range[0] == '\r'
		    or source->getLineColumn(start).second == 1)
			code_list.emplace_back(makeBox<dia_args::StartLineComponent>(lines[0].first));

		code_list.emplace_back(
			makeBox<dia_args::CodeComponent>(NORMALIZE(lines[0].second.stdString()))
		);

		for (usize i = 1; i < lines.size(); i++) {
			code_list.emplace_back(makeBox<dia_args::StartLineComponent>(lines[i].first));
			code_list.emplace_back(
				makeBox<dia_args::CodeComponent>(NORMALIZE(lines[i].second.stdString()))
			);
		}
	}

	/**
	 * @brief Return pointer messages that are within the given snippet position.
	 * dia::SourcePosition is inclusive as always.
	 */
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

	Box<dia_args::Component> CodeArgument::getValue(MessageBase& diag) {
		auto source = position.getSource();

		// [start_line, end_line] is the minimal range of lines containing the code.
		usize start_line = position.getStartLineColumn().first;
		usize end_line   = position.getEndLineColumn().first;

		// [first_line, last_line] is the range of lines containing the code extended by additional
		// context lines.
		usize first_line = std::max(1 + lines_before, start_line) - lines_before;
		usize last_line  = std::min(source->getLines().size(), end_line + lines_after);

		// [begin_char, end_char] is the range of characters in lines [first_line, last_line].
		usize begin_char = source->getLine(first_line).first;
		usize end_char   = source->getLine(last_line).second;

		// This should only happen for an empty location.
		if (begin_char == 0 && end_char == 0) return base::makeBox<dia_args::ConcatComponent>();

		// The list where we collect the code components
		auto code_list = std::vector<Box<dia_args::Component>>();

		// @TODO: #1792 Fix the handling of message pointer to EOF.
		const auto pointer_messages = filterMessages(
			diag.getPointerMessages(),
			dia::SourcePosition(position.getLocation(), begin_char, end_char - 1)
		);

		if (pointer_messages.empty()) {
			addCodeLines(code_list, source, begin_char, end_char);
			return base::makeBox<dia_args::ConcatComponent>(std::move(code_list));
		}

		struct PointerMessageID {
			std::string                 name;
			base::Optional<std::string> message_id;

			bool operator==(const PointerMessageID&) const = default;
		};

		// Build sorted list of unique edge positions.
		// Each edge tracks which pointer messages start/end there.
		struct Edge {
			usize                         pos{};
			std::vector<PointerMessageID> starting;  // messages starting at this edge
			std::vector<PointerMessageID> ending;    // messages ending at this edge
		};

		std::map<usize, Edge> edge_map;
		for (const auto& pm: pointer_messages) {
			usize pm_start = pm.position.getStart();
			usize pm_end   = pm.position.getEnd() + 1;

			edge_map[pm_start].pos = pm_start;
			edge_map[pm_start].starting.push_back(PointerMessageID{ .name = pm.pointer_message_id,
			                                                        .message_id = pm.message_id });

			edge_map[pm_end].pos = pm_end;
			edge_map[pm_end].ending.push_back(PointerMessageID{ .name       = pm.pointer_message_id,
			                                                    .message_id = pm.message_id });
		}

		// Convert to sorted vector for iteration.
		std::vector<Edge> edges;
		edges.reserve(edge_map.size());
		for (auto& [pos, edge]: edge_map) edges.push_back(std::move(edge));
		std::ranges::sort(edges, [](const Edge& a, const Edge& b) { return a.pos < b.pos; });

		// Track currently active pointer messages.
		std::vector<PointerMessageID> active_messages;

		usize current_pos = begin_char;

		for (const auto& edge: edges) {
			// Add plain code segment before this edge.
			if (current_pos < edge.pos) {
				if (active_messages.empty()) {
					addCodeLines(code_list, source, current_pos, edge.pos);
				} else {
					// This segment is highlighted by active messages.
					auto highlighted = std::vector<Box<dia_args::Component>>();
					addCodeLines(highlighted, source, current_pos, edge.pos);

					std::vector<dia_args::PointerMessage> ptr_msgs;

					ptr_msgs.reserve(active_messages.size());
					for (const auto& name: active_messages)
						ptr_msgs.emplace_back(name.name, name.message_id);

					code_list.emplace_back(base::makeBox<dia_args::PointedComponent>(
						makeBox<dia_args::ConcatComponent>(std::move(highlighted)),
						std::move(ptr_msgs)
					));
				}
			}

			// Process endings before starts (close spans first).
			for (const auto& name: edge.ending) {
				auto it = std::ranges::find(active_messages, name);
				if (it != active_messages.end()) active_messages.erase(it);
			}

			// Add new starting messages.
			for (const auto& name: edge.starting) active_messages.push_back(name);

			current_pos = edge.pos;
		}

		// Add remaining code after last edge.
		if (current_pos < end_char) addCodeLines(code_list, source, current_pos, end_char);

		return base::makeBox<dia_args::ConcatComponent>(std::move(code_list));
	}

	dia_args::Message MessageBase::buildMessages(
		base::HashMap<std::string, dia_args::Message>& additional_messages
	) {
		dia_args::Message msg;
		msg.metadata = getMetadata();
		for (const auto& arg: arguments) msg.arguments.put(arg->getName(), arg->getValue(*this));
		for (const auto& link: explore_links) msg.explore_links.push_back(link.getValue(*this));

		msg.attached_messages.reserve(this->attached_messages.size());

		for (const auto& attached_msg: attached_messages)
			msg.attached_messages.push_back(attached_msg);

		for (const auto& [id, value]: this->linked_messages)
			additional_messages.put(id, value->buildMessages(additional_messages));

		return msg;
	}

	Box<dia_int::dia_args::Diagnostic> MessageBase::buildDiagnosticFile() {
		if (has_been_built) CORE_PANIC("Message can only be built once!");
		has_been_built = true;

		Box<dia_args::Diagnostic>                     thread = makeBox<dia_args::Diagnostic>();
		base::HashMap<std::string, dia_args::Message> additional_messages;

		thread->main_message    = buildMessages(additional_messages);
		thread->linked_messages = std::move(additional_messages);

		return thread;
	}

	std::string MessageBase::getUniqueID() {
		static usize counter = 0;
		return base::strConcat("msg_", counter++);
	}

	dia_args::ExploreLink ExploreLink::getValue(MessageBase& message) const {
		dia_args::ExploreLink edge;
		for (const auto& arg: arguments) edge.params.put(arg->getName(), arg->getValue(message));
		edge.name = message_id;
		return edge;
	}

	MessageWithCodeFragment::MessageWithCodeFragment(dia::SourcePosition source_position) {
		addArgument<CodeArgument>("code", source_position);
		addArgument<CodeLocationArgument>("code_location", source_position);
	}

	MessageWithCodeFragmentAndCause::MessageWithCodeFragmentAndCause(
		dia::SourcePosition source_position
	) {
		addArgument<CodeArgument>("code", source_position);
		addArgument<CodeLocationArgument>("code_location", source_position);
		addPointerMessage({ "cause", source_position });
	}

	bool MessageBase::isError() const {
		auto meta = getMetadata();
		return meta.type == "error";
	}
}

DEFAULT_BOX_PTR_DELETER_DEFINITION(dia_int::MessageBase);
