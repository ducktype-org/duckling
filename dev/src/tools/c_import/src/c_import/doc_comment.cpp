// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "doc_comment.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

namespace c_import {

	namespace {

		constexpr std::array<std::string_view, 8> OPENING_MARKERS{
			"/**<", "/*!<", "///<", "//!<", "/**", "/*!", "///", "//!",
		};

		constexpr std::array<std::string_view, 4> TRAILING_MARKERS{ "/**<", "/*!<", "///<", "//!<" };

		std::vector<std::string_view> splitLines(std::string_view text) {
			std::vector<std::string_view> lines;
			while (true) {
				auto end  = text.find('\n');
				auto line = text.substr(0, end);
				if (line.ends_with('\r')) line.remove_suffix(1);
				lines.push_back(line);
				if (end == std::string_view::npos) return lines;
				text.remove_prefix(end + 1);
			}
		}

		std::string_view trimLeft(std::string_view text) {
			auto start = text.find_first_not_of(" \t");
			return start == std::string_view::npos ? std::string_view{} : text.substr(start);
		}

		std::string_view trimRight(std::string_view text) {
			auto end = text.find_last_not_of(" \t");
			return end == std::string_view::npos ? std::string_view{} : text.substr(0, end + 1);
		}

		/// One comment line without its markers, keeping the indentation after them.
		std::string_view stripMarkers(std::string_view line) {
			line = trimLeft(line);
			for (auto marker: OPENING_MARKERS) {
				if (line.starts_with(marker)) {
					line.remove_prefix(marker.size());
					break;
				}
			}
			if (line.starts_with("*/"))
				line = {};
			else if (line.starts_with('*'))
				line.remove_prefix(1);
			line = trimRight(line);
			if (line.ends_with("*/")) line = trimRight(line.substr(0, line.size() - 2));
			// A rule such as `/*********` or `//////` carries no text.
			if (line.find_first_not_of("*/") == std::string_view::npos) return {};
			return line;
		}

		std::size_t utf8SequenceLength(std::string_view text, std::size_t at) {
			auto        lead   = static_cast<unsigned char>(text[at]);
			std::size_t length = 0;
			if (lead < 0x80) return 1;
			if ((lead & 0xE0U) == 0xC0U && lead >= 0xC2U)
				length = 2;
			else if ((lead & 0xF0U) == 0xE0U)
				length = 3;
			else if ((lead & 0xF8U) == 0xF0U && lead <= 0xF4U)
				length = 4;
			else
				return 0;
			if (at + length > text.size()) return 0;
			for (std::size_t k = 1; k < length; ++k)
				if ((static_cast<unsigned char>(text[at + k]) & 0xC0U) != 0x80U) return 0;
			return length;
		}

		/// `text` with every byte that is not part of a valid UTF-8 sequence replaced by `?`.
		std::string validUtf8(std::string_view text) {
			std::string result;
			result.reserve(text.size());
			for (std::size_t at = 0; at < text.size();) {
				auto length = utf8SequenceLength(text, at);
				if (length == 0) {
					result += '?';
					++at;
				} else {
					result.append(text.substr(at, length));
					at += length;
				}
			}
			return result;
		}

	}

	std::string cleanDocComment(std::string_view raw) {
		std::vector<std::string_view> lines;
		for (auto line: splitLines(raw)) lines.push_back(stripMarkers(line));

		auto indent = std::numeric_limits<std::size_t>::max();
		for (auto line: lines)
			if (!line.empty()) indent = std::min(indent, line.find_first_not_of(" \t"));

		auto        first = std::ranges::find_if(lines, [](auto line) { return !line.empty(); });
		auto        last  = std::ranges::find_if(lines.rbegin(), lines.rend(), [](auto line) {
                        return !line.empty();
                    }).base();
		std::string text;
		for (auto it = first; it < last; ++it) {
			if (it != first) text += '\n';
			if (!it->empty()) text += it->substr(indent);
		}
		return validUtf8(text);
	}

	std::string macroDocComment(std::string_view source, std::size_t line) {
		auto lines = splitLines(source);
		if (line == 0 || line > lines.size()) return {};
		auto definition = lines[line - 1];
		for (auto marker: TRAILING_MARKERS)
			if (auto at = definition.find(marker); at != std::string_view::npos)
				return cleanDocComment(definition.substr(at));

		std::size_t end = line - 1;  // One past the comment's last line.
		if (end == 0) return {};
		auto        above = trimLeft(lines[end - 1]);
		std::size_t begin = end - 1;
		if (above.starts_with("///") || above.starts_with("//!")) {
			while (begin > 0) {
				auto previous = trimLeft(lines[begin - 1]);
				if (!previous.starts_with("///") && !previous.starts_with("//!")) break;
				--begin;
			}
		} else if (trimRight(above).ends_with("*/")) {
			while (!lines[begin].contains("/*")) {
				if (begin == 0) return {};
				--begin;
			}
			auto opening = trimLeft(lines[begin]);
			if (!opening.starts_with("/**") && !opening.starts_with("/*!")) return {};
		} else {
			return {};
		}

		std::string comment;
		for (auto k = begin; k < end; ++k) {
			comment.append(lines[k]);
			comment += '\n';
		}
		return cleanDocComment(comment);
	}

}
