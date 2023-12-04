/**
 * @file source_position.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include "source_position.hpp"
#include "base/exceptions.hpp"

#include <utility>

std::string SourcePosition::getSourceChars() const {
	auto             source_content = source_code->getContent();
	std::string_view source         = source_content.view().stringView();

	if (source_end == source.size()) return "<EOF>";
	return std::string(source.begin() + source_start, source.begin() + source_end + 1);
}

SourcePosition::SourcePosition(
	Source source_code_, usize line_, usize column_, usize source_start_
):
		SourcePosition(source_code_, line_, column_, source_start_, source_start_) {}

SourcePosition::SourcePosition(
	Source source_code_, usize line_, usize column_, usize source_start_, usize source_end_
):
		line(line_), column(column_), 
		source_start(source_start_), source_end(source_end_),
		source_code(source_code_) {
	if (!source_code_) throw base::LogicError("Invalid SourcePosition: No such file");
	if (line == 0) throw base::LogicError("Invalid SourcePosition: line = 0");
	if (column == 0) throw base::LogicError("Invalid SourcePosition: column = 0");
	if (source_end < source_start)
		throw base::LogicError("Invalid SourcePosition: source end before source start");
	// allow EOF position
	if (not (source_end == source_start and source_end == source_code->getContent().size()))
		if (source_end >= source_code->getContent().size())
			throw base::LogicError("Invalid SourcePosition: source end outside the file");
}
SourcePosition::SourcePosition(const SourcePosition& other):
	line(other.line), column(other.column), 
	source_start(other.source_start), source_end(other.source_end), 
	source_code(other.source_code) {}

usize SourcePosition::getColumn() const { return column; }
usize SourcePosition::getLine() const { return line; }
usize SourcePosition::getStart() const { return source_start; }
usize SourcePosition::getEnd() const { return source_end; }
SourcePosition::Source SourcePosition::getSource() const { return source_code; }

printer::Message SourcePosition::genErrorMsg(std::string_view reason) const {
	return printer::Message({
		{
			{ "In file: " },
			{ source_code->strView().data() },
			{ ":" + std::to_string(line) + ":" + std::to_string(column) + "\n"},
			{ "error: " , printer::Color::BRIGHT_RED},
			{ reason.data() },
			{ "\n" },
			{ "  |\n" },
			{ std::to_string(line) },
			{ " | " },
			{ getSourceChars() + "\n" },
			{ "  |\n" }
		},
		printer::MessageType::ERROR
	});
}

std::string SourcePosition::genErrorStr(std::string_view reason) const {
	std::string output = "In file: ";
	output += source_code->strView();
	output += ":" + std::to_string(line) + ":" + std::to_string(column) + "\n";
	output += "error: ";
	output += reason;
	output += "\n";
	output += "  |\n";
	output += std::to_string(line);
	output += " | ";
	output += getSourceChars() + "\n";
	output += "  |\n";
	return output;
}