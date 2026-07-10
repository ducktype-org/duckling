// Playground: render code-dependency highlights of a query as a standalone HTML page.
//
// It compiles a Duckling source root, picks ONE entity (function or variable),
// asks the PST layer which source positions a chosen query depends on, and emits
// an HTML document where those positions are wrapped in <span class="dep">.
//
// Meant for blog visuals: pipe stdout to a file and open it in a browser.
//     ./bin/code_dep_html_playground -p <src-root> > deps.html
//
// ============================ HOW TO SWAP THE TARGET ============================
// Two example selectors are provided below (see depsForFunction / depsForVariable).
// Edit `selectDependencies()` near the bottom to choose which query on which
// symbol you want to visualize. That is the only place you should need to touch.
// ================================================================================

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/pst_query/code_dependency.hpp>

#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>

#include <diagnostic/source_position.hpp>
#include <token_source/source.hpp>

#include <clah/clah.hpp>
#include <init/init.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>

#include <string_id/string_id.hpp>

#include <algorithm>
#include <iostream>
#include <ranges>
#include <string>
#include <tuple>
#include <vector>

using namespace compiler;

namespace {

	// -------------------------------------------------------------------------
	// Selectors: each maps a named entity in the module to the positions that a
	// given query depends on. Add your own by copying one of these.
	// -------------------------------------------------------------------------

	/// QueryDeclOfFun on a function found by name.
	[[maybe_unused]] std::vector<dia::SourcePosition>
	depsForFunction(const helios::HOUTUnit& unit, base::StrID name) {
		for (auto& fun: unit.functions) {
			if (fun->declaration->original_name == name) {
				return pst::queryPositionDependencies<helios::QueryDeclOfFun>(
					fun->declaration->original_symbol
				);
			}
		}
		std::cerr << "warning: no function named '" << name.strView() << "' found\n";
		return {};
	}

	/// QueryTypeOfSymbol on a global variable/constant found by name.
	[[maybe_unused]] std::vector<dia::SourcePosition>
	depsForVariable(const helios::HOUTUnit& unit, base::StrID name) {
		for (auto& glob: unit.glob_data) {
			if (glob->original_name == name) {
				return pst::queryPositionDependencies<helios::QueryTypeOfSymbol>(
					glob->helios_symbol
				);
			}
		}
		std::cerr << "warning: no global variable named '" << name.strView() << "' found\n";
		return {};
	}

	// -------------------------------------------------------------------------
	// HTML emission
	// -------------------------------------------------------------------------

	std::string htmlEscape(std::string_view s) {
		std::string out;
		out.reserve(s.size());
		for (char c: s) {
			switch (c) {
				case '&': out += "&amp;"; break;
				case '<': out += "&lt;"; break;
				case '>': out += "&gt;"; break;
				default: out += c;
			}
		}
		return out;
	}

	constexpr std::string_view HTML_HEAD = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Query code dependencies</title>
<style>
  :root { color-scheme: light dark; }
  body {
    font: 14px/1.5 ui-monospace, SFMono-Regular, "JetBrains Mono", Menlo, monospace;
    margin: 2rem auto; max-width: 900px; padding: 0 1rem;
    background: #fbfbfd; color: #1c1c1e;
  }
  h1 { font-size: 1.1rem; font-weight: 600; }
  .file { border: 1px solid #d8d8de; border-radius: 8px; overflow: hidden; margin: 1.25rem 0; }
  .file-name {
    background: #ececef; padding: .5rem .9rem; font-weight: 600; font-size: .85rem;
    border-bottom: 1px solid #d8d8de;
  }
  table.code { border-collapse: collapse; width: 100%; }
  table.code td { padding: 0 .4rem; vertical-align: top; white-space: pre; }
  td.ln {
    text-align: right; color: #9a9aa0; user-select: none;
    border-right: 1px solid #e2e2e6; width: 1%; padding-right: .8rem;
  }
  td.src { padding-left: .8rem; width: 100%; }
  .dep {
    background: rgba(191, 90, 242, .22);
    border-radius: 3px;
    box-shadow: 0 0 0 1px rgba(191, 90, 242, .45);
  }
  @media (prefers-color-scheme: dark) {
    body { background: #16161a; color: #e6e6ea; }
    .file { border-color: #2c2c33; }
    .file-name { background: #202028; border-color: #2c2c33; }
    td.ln { color: #6a6a72; border-color: #2c2c33; }
    .dep { background: rgba(191, 90, 242, .28); box-shadow: 0 0 0 1px rgba(191, 90, 242, .5); }
  }
</style>
</head>
<body>
<h1>Query code dependencies</h1>
)HTML";

	constexpr std::string_view HTML_TAIL = "</body>\n</html>\n";

	/// Emit one file block, highlighting every position of @p positions that
	/// belongs to @p source. Mirrors the chunking of dia::printHighlightedPositions
	/// but produces HTML rows instead of ANSI-colored terminal output.
	void emitFileHtml(
		std::ostream&                     out,
		Ref<tokenizer::TokenSource>       source,
		const std::vector<dia::SourcePosition>& all_positions
	) {
		using namespace std::views;

		usize total_lines = source->getLines().size();
		if (total_lines == 0) return;

		usize start_char = source->getLine(1).first;
		usize end_char   = source->getLine(total_lines).second;

		std::vector<dia::SourcePosition> positions;
		for (auto& p: all_positions)
			if (p.getSource() == source) positions.push_back(p);
		std::ranges::sort(positions);

		// (line, text, is_dependency)
		std::vector<std::tuple<usize, std::string, bool>> colored;
		usize cursor = start_char;

		for (auto& pos: positions) {
			usize p_start = pos.getStart();
			usize p_end   = pos.getEnd() + 1;  // exclusive

			if (p_end <= cursor) continue;         // already emitted / behind cursor
			p_start = std::max(p_start, cursor);   // clamp overlaps

			if (p_start > cursor) {
				for (auto& [line, view]: source->viewSplitRange(cursor, p_start))
					colored.emplace_back(line, view.stdString(), false);
			}

			p_end = std::min(p_end, end_char);
			if (p_end > p_start) {
				for (auto& [line, view]: source->viewSplitRange(p_start, p_end))
					colored.emplace_back(line, view.stdString(), true);
			}
			cursor = std::max(cursor, p_end);
		}
		if (cursor < end_char) {
			for (auto& [line, view]: source->viewSplitRange(cursor, end_char))
				colored.emplace_back(line, view.stdString(), false);
		}

		out << "<div class=\"file\"><div class=\"file-name\">"
		    << htmlEscape(source->getFile().getFilePath().native()) << "</div>\n";
		out << "<table class=\"code\">\n";

		usize current_line = (usize) -1;
		bool  row_open     = false;
		for (auto& [line, text, is_dep]: colored) {
			if (line != current_line) {
				if (row_open) out << "</td></tr>\n";
				out << "<tr><td class=\"ln\">" << line << "</td><td class=\"src\">";
				current_line = line;
				row_open     = true;
			}
			if (is_dep)
				out << "<span class=\"dep\">" << htmlEscape(text) << "</span>";
			else
				out << htmlEscape(text);
		}
		if (row_open) out << "</td></tr>\n";
		out << "</table></div>\n";
	}

	void emitHtml(std::ostream& out, const std::vector<dia::SourcePosition>& positions) {
		out << HTML_HEAD;

		// Distinct sources, in first-seen order.
		std::vector<Ref<tokenizer::TokenSource>> sources;
		for (auto& p: positions) {
			auto s = p.getSource();
			if (std::ranges::find(sources, s) == sources.end()) sources.push_back(s);
		}
		if (sources.empty()) out << "<p>No dependencies found.</p>\n";
		for (auto& s: sources) emitFileHtml(out, s, positions);

		out << HTML_TAIL;
	}

	// -------------------------------------------------------------------------
	// >>>>>>>>>>>>>>>>>>>>>>>>> SWAP THE VISUALIZED QUERY HERE <<<<<<<<<<<<<<<<<<<
	// -------------------------------------------------------------------------
	std::vector<dia::SourcePosition> selectDependencies(const helios::HOUTUnit& unit) {
		// Visualize QueryDeclOfFun of function `main`:
		return depsForFunction(unit, base::StrID("main"));

		// Or visualize QueryTypeOfSymbol of a global variable named `my_var`:
		// return depsForVariable(unit, base::StrID("my_var"));
	}
	// -------------------------------------------------------------------------

	int notMain(int argc, const char* const* argv) {
		init::InitObject _;

		auto clah = clah::Clah("code_dep_html_playground")
		                .add(clah::ParamBuilder::ofValue(clah::FileParser::make("Path"))
		                         .addShortName('p')
		                         .addShortDesc("Path to Duckling source root")
		                         .required()
		                         .build());

		clah::ParsingResult options;
		try {
			options = clah.parse(usize(argc), argv);
		} catch (clah::exceptions::HelpException& e) {
			std::cerr << clah::HelpMessageGenerator::generate(clah, e.parsing_result) << '\n';
			return 1;
		} catch (clah::exceptions::ClahException& e) {
			std::cerr << e.what() << '\n';
			return 1;
		}

		auto path_to_compile = options.getValue<fs::File>('p').value();

		auto  root = frontend::createModuleTreeWithRandomPackageID(path_to_compile);
		auto& unit = query::entryPoint<helios::QueryModuleHOUT>(root)->valueOrPanic();

		auto positions = selectDependencies(unit);
		emitHtml(std::cout, positions);
		return 0;
	}

}

int main(int argc, const char* argv[]) {
	// Catch here so stack unwinding (and defers) always runs.
	try {
		return notMain(argc, argv);
	} catch (std::exception& e) {
		std::cerr << "exception was thrown: " << e.what() << '\n';
		return 1;
	}
}
