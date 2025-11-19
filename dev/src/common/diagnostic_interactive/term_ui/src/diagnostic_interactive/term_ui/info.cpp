#include "info.hpp"

namespace term_ui {
	Info::Info(
		StyleType          type,
		u32                id,
		const std::string& message,
		const std::string& description,
		const CodeSection& code
	):
		  type(type),
		  id(id) {
		main_section = message;
		sections.emplace_back(code);
		if (!description.empty()) sections.emplace_back(description);
	}

	Info::Info(const view::Info& info) {
		// Extract metadata.
		type = style_type_of(info.metadata().type());
		id   = info.metadata().code();

		// Extract sections.
		main_section = TextPieces(info.header()).to_string();
		for (u32 i = 0; i < info.sections_size(); ++i) {
			if (info.sections(i).has_text_section()) {
				auto& section = info.sections(i).text_section();
				sections.emplace_back(TextPieces(section).to_string());
			} else {
				auto& section = info.sections(i).code_section();
				sections.emplace_back(CodeSection(section));
			}
		}
	}

	void Info::print(std::ostream& out) const {
		Style style = get_style(type);
		style.printName(id, out);
		style.printWith(":", out);

		out << ' ';
		style.printMainWith(main_section, out);
		out << '\n';

		for (auto& section: sections) {
			if (std::holds_alternative<TextSection>(section)) {
				out << std::get<TextSection>(section) << '\n' << '\n';
			} else {
				std::get<CodeSection>(section).print(out);
				out << '\n';
			}
		}
	}
}
