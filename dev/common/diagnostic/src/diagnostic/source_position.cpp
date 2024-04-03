/**
 * @file source_position.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */


#include "source_position.hpp"
#include "base/exceptions.hpp"

namespace dia {
	std::string SourcePosition::getSourceChars() const {
		auto             source_content = source_file->getContent();
		std::string_view source         = source_content.view().stringView();

		if (source_end == source.size()) return "<EOF>";
		return { source.begin() + source_start, source.begin() + source_end + 1 };
	}

	SourcePosition::SourcePosition(
		const SourceFile& source_file,
		const usize       line,
		const usize       column,
		const usize       source_start
	):
		  SourcePosition(source_file, line, column, source_start, source_start) {}

	SourcePosition::SourcePosition(
		const SourceFile& source_file,
		const usize       line,
		const usize       column,
		const usize       source_start,
		const usize       source_end
	):
		  line(line),
		  column(column),
		  source_start(source_start),
		  source_end(source_end),
		  source_file(source_file) {
		if (!source_file) throw base::LogicError("Invalid SourcePosition: No such file");
		if (line == 0) throw base::LogicError("Invalid SourcePosition: line = 0");
		if (column == 0) throw base::LogicError("Invalid SourcePosition: column = 0");
		if (source_end < source_start)
			throw base::LogicError("Invalid SourcePosition: source end before source start");
		// allow EOF position
		if (not(source_end == source_start and source_end == source_file->getContent().size())) {
			if (source_end >= source_file->getContent().size())
				throw base::LogicError("Invalid SourcePosition: source end outside the file");
		}
	}

	SourcePosition::SourcePosition(const SourcePosition& other, const usize source_end):
		  SourcePosition(
			  other.source_file, other.line, other.column, other.source_start, source_end
		  ) {}

	usize SourcePosition::getColumn() const { return column; }

	usize SourcePosition::getLine() const { return line; }

	usize SourcePosition::getStart() const { return source_start; }

	usize SourcePosition::getEnd() const { return source_end; }

	SourcePosition::SourceFile SourcePosition::getSource() const { return source_file; }

	std::vector<printer::MessageContent>
		SourcePosition::genPrinterMessageContents(const printer::MessageContent& reason) const {
		return { { "In file: " },
			     { source_file->strView().data() },
			     { ":" + std::to_string(line) + ":" + std::to_string(column) + "\n" },
			     reason,
			     { "\n" },
			     { "  |\n" },
			     { std::to_string(line) },
			     { " | " },
			     { getSourceChars() + "\n" },
			     { "  |\n" } };
	}

	std::string SourcePosition::genStr(const std::string_view reason) const {
		std::string output = "In file: ";
		output += source_file->strView();
		output += ":" + std::to_string(line) + ":" + std::to_string(column) + "\n";
		output += reason;
		output += "\n";
		output += "  |\n";
		output += std::to_string(line);
		output += " | ";
		output += getSourceChars() + "\n";
		output += "  |\n";
		return output;
	}
}
