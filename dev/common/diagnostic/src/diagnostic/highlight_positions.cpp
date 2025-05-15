#include "highlight_positions.hpp"

#include <token_source/source.hpp>

#include <algorithm>
#include <format>
#include <ranges>

namespace dia {
	/**
	 * @brief Prints specified lines highlighted
	 *
	 * @param out Output printer.
	 * @param source Source to print from.
	 * @param start_line Line at which to start.
	 * @param end_line Line at which to end.
	 * @param positions Positions to highlight, assumed to be sorted.
	 * @param line_col Line number color.
	 * @param high_col Highlight color.
	 */
	static void printLinesWithHighlights(
		printer::PrinterOStream&           out,
		Ref<tokenizer::TokenSource>        source,
		usize                              start_line,
		usize                              end_line,
		const std::vector<SourcePosition>& positions,
		printer::Color                     line_col,
		printer::Color                     high_col
	) {
		using namespace std::views;

		CORE_ASSERT(start_line > 0, "Start line out of range.");
		CORE_ASSERT(start_line > 0, "End line out of range.");
		CORE_ASSERT(start_line <= source->getLines().size(), "Start line out of range.");
		CORE_ASSERT(end_line <= source->getLines().size(), "End line out of range.");
		CORE_ASSERT(start_line <= end_line, "End line before start line.");

		usize length = std::to_string(end_line).size();

		auto matching_pos = [&](SourcePosition pos) { return pos.getSource() == source; };

		auto interesting_positions = positions | filter(matching_pos);

		usize current_pos = source->getLine(start_line).first;
		usize bound_pos   = source->getLine(end_line).second;

		// Just the code with colors but without numbered lines. (line nr, code, color)
		std::vector<std::tuple<usize, base::RawView, base::Optional<printer::Color>>> colored;

		for (auto pos: interesting_positions) {
			if (pos.getEnd() < current_pos || pos.getStart() >= bound_pos) continue;
			if (pos.getStart() > current_pos) {
				auto code_lines
					= source->viewSplitRange(current_pos, std::min(bound_pos, pos.getStart()));

				for (auto& [line, view]: code_lines) colored.emplace_back(line, view, std::nullopt);
				current_pos = std::min(pos.getStart(), bound_pos);
			}
			if (current_pos == bound_pos) break;

			auto code_lines
				= source->viewSplitRange(current_pos, std::min(bound_pos, pos.getEnd() + 1));

			for (auto& [line, view]: code_lines) colored.emplace_back(line, view, high_col);
			current_pos = std::min(bound_pos, pos.getEnd() + 1);
		}
		if (current_pos < bound_pos) {
			auto code_lines = source->viewSplitRange(current_pos, bound_pos);

			for (auto& [line, view]: code_lines) colored.emplace_back(line, view, std::nullopt);
			current_pos = bound_pos;
		}

		usize old{ (usize) -1 };

		for (auto& [line, view, col]: colored) {
			if (old != line) {
				if (old != (usize) -1) out << "\n";
				old = line;
				printLineNumber(out, length, line, line_col);
			}
			if (col)
				out.add({ view.stdString(), col.value() });
			else
				out << view.stdString();
		}
		out << "\n";
	}

	static constexpr usize safeMinus(usize a, usize b) {
		if (a <= b) return 1;
		return a - b;
	}

	static constexpr usize safePlus(usize a, usize b) {
		if (a + b < a) return (usize) -1;
		return a + b;
	}

	void printHighlightedPositions(
		printer::PrinterOStream&           out,
		const std::vector<SourcePosition>& poss,
		usize                              neighborhood,
		printer::Color                     line_col,
		printer::Color                     high_col
	) {
		using namespace std::views;

		std::vector<SourcePosition> positions{ poss };

		std::ranges::sort(positions);

		auto split_positions
			= positions | chunk_by([&](auto one, auto oth) {
				  return one.getLocation() == oth.getLocation()
			          && safePlus(
							 safePlus(
								 safePlus(one.getEndLineColumn().first, neighborhood), neighborhood
							 ),
							 1
						 ) >= oth.getEndLineColumn().first;
			  });

		bool nl = false;
		for (auto chunk: split_positions) {
			if (nl)
				out << "\n";
			else
				nl = true;

			auto file = chunk.back().getSource()->getPath();

			auto first_line = safeMinus(chunk.front().getStartLineColumn().first, neighborhood);
			auto last_line  = std::min(
                safePlus(chunk.back().getEndLineColumn().first, neighborhood),
                chunk.back().getSource()->getLines().size()
            );

			out << "File: " << file.absolutePath() << "\n";
			out << std::format("Lines: {}-{}\n", first_line, last_line);

			std::vector<SourcePosition> sub_positions;
			for (auto pos: chunk) sub_positions.push_back(pos);

			printLinesWithHighlights(
				out, chunk.front().getSource(), first_line, last_line, sub_positions, line_col, high_col
			);
		}
	}
}
