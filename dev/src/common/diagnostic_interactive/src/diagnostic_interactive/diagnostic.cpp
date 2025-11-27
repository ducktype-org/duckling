#include "diagnostic.hpp"

#include "diagnostic_interactive/core/diagnostic_file.hpp"

namespace dia_int {

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

	Box<dia_file::Component> CodeArgument::getValue(DiagnosticBase& diag) {
		// Here would be a lot of code to extract the code fragment from the source file.
		// And potentially add interactive contents.
		auto source = position.getSource();

		usize start_line = position.getStartLineColumn().first;
		usize end_line   = position.getEndLineColumn().first;
		std::cout << "start_line: " << start_line << ", end_line: " << end_line << "\n";

		usize first_line = std::max(1 + lines_before, start_line) - lines_before;
		usize last_line  = std::min(source->getLines().size(), end_line + lines_after);

		std::cout << "first_line: " << first_line << ", last_line: " << last_line << "\n";

		usize begin_char = source->getLine(first_line).first;
		usize end_char   = source->getLine(last_line).second;


		auto code_list = std::vector<Box<dia_file::Component>>();

		const auto& pointer_messages = diag.getPointerMessages();
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
}
