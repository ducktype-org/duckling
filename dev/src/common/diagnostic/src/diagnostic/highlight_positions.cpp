// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "highlight_positions.hpp"

#include <token_source/source.hpp>

#include <algorithm>
#include <format>
#include <ranges>

namespace dia {
	/**
	 * @brief Prints specified lines from a single source highlighted.
	 *
	 * @note This function should work on any set of positions, but test it properly before exporting.
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
			// If the current_position is past the end of pos then we skip the pos.
			if (pos.getEnd() < current_pos || pos.getStart() >= bound_pos) continue;
			// If the current_position is before the position then we add the [current_pos,
			// min(pos.start, bound_pos)) chunk of code uncolored.
			if (pos.getStart() > current_pos) {
				auto code_lines
					= source->viewSplitRange(current_pos, std::min(bound_pos, pos.getStart()));

				for (auto& [line, view]: code_lines) colored.emplace_back(line, view, std::nullopt);
				current_pos = std::min(pos.getStart(), bound_pos);
			}
			// If the current position is at the bound position end.
			if (current_pos == bound_pos) break;

			// Add the [current_pos, min(pos.end, bound_pos)) chunk colored
			auto code_lines
				= source->viewSplitRange(current_pos, std::min(bound_pos, pos.getEnd() + 1));

			for (auto& [line, view]: code_lines) colored.emplace_back(line, view, high_col);
			current_pos = std::min(bound_pos, pos.getEnd() + 1);
		}
		// Add the final chunk of code uncolored if needed.
		if (current_pos < bound_pos) {
			auto code_lines = source->viewSplitRange(current_pos, bound_pos);

			for (auto& [line, view]: code_lines) colored.emplace_back(line, view, std::nullopt);
		}

		// Print the chunks with line numbers.
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

	namespace {
		/**
		 * @brief Substraction that safely operates on line numbers.
		 */
		constexpr usize safeMinus(const usize a, const usize b) {
			if (a <= b) return 1;
			return a - b;
		}

		/**
		 * @brief Addition that safely operates on line numbers.
		 */
		constexpr usize safePlus(const usize a, const usize b) {
			if (a + b < a) return (usize) -1;
			return a + b;
		}
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

		// This is basically the logic to check if two positions would merge into a single chunk of
		// code to print
		const auto same_group = [&](auto one, auto oth) {
			return one.getLocation() == oth.getLocation()
			    && safePlus(
					   safePlus(safePlus(one.getEndLineColumn().first, neighborhood), neighborhood),
					   1
				   ) >= oth.getEndLineColumn().first;
		};

		auto split_positions = positions | chunk_by(same_group);

		bool nl = false;
		for (auto chunk: split_positions) {
			if (nl)
				out << "\n";
			else
				nl = true;

			auto file = chunk.back().getSource()->getFile();

			// Calculate the bounds [first_line, last_line] of printing in this chunk
			auto first_line = safeMinus(chunk.front().getStartLineColumn().first, neighborhood);
			auto last_line  = std::min(
                safePlus(chunk.back().getEndLineColumn().first, neighborhood),
                chunk.back().getSource()->getLines().size()
            );

			out << "File: " << file.getFilePath().native() << "\n";
			out << std::format("Lines: {}-{}\n", first_line, last_line);

			std::vector<SourcePosition> sub_positions;
			for (auto pos: chunk) sub_positions.push_back(pos);

			printLinesWithHighlights(
				out, chunk.front().getSource(), first_line, last_line, sub_positions, line_col, high_col
			);
		}
	}
}
