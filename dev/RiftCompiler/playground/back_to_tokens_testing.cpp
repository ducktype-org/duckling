#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <pst_parser/pst.hpp>
#include <base/variant.hpp>
#include <iostream>

void handleToken(pst::RiftElement::SubToken token) { std::cerr << " " << token->getStrValue(); }

void handleElement(pst::ParserCBorrowRef<pst::RiftElement> el) {
	for (auto sub: el->viewSubElements()) {
		variant_match(sub) {
			variant_case(pst::RiftElement::SubToken, token) { handleToken(token); }
			variant_case(pst::RiftElement::Child, child) { handleElement(child); }
		}
	}
}

usize next() {
	static usize id = 0;
	return id++;
}

usize dotElement(pst::ParserCBorrowRef<pst::RiftElement> el) {
	auto self = next();
	for (auto sub: el->viewSubElements()) {
		variant_match(sub) {
			variant_case(pst::RiftElement::SubToken, token) {
				auto sub_id = next();
				std::cerr << self << " -> " << sub_id << ";\n";
				std::cerr << sub_id << " [shape=box];\n";
				std::cerr << sub_id << " [label=\"" << token->getStrValue() << "\"];\n";
			}
			variant_case(pst::RiftElement::Child, child) {
				usize sub_id = dotElement(child);
				std::cerr << self << "->" << sub_id << ";\n";
			}
		}
	}
	return self;
}

void pre() { std::cerr << "digraph pst {\n"; }

void post() { std::cerr << "}\n"; }

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: ./back_to_tokens_testing file_name\n";
		return 1;
	}
	pst::init();
	fs::FilePath file(argv[1]);
	pst::PST<>   pst(file);

	if (pst.getLogger().bad()) {
		pst.getLogger().dumpLog(false, std::cerr);
		std::cerr << "\nThere are errors, aborting.\n";
		pst.dprint(std::cerr);
		std::cerr << "\n";
	} else {
		pre();
		dotElement(pst.getRootElement());
		post();
	}
}
