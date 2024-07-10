#include <filesystem/file.hpp>
#include <lexer/lexer.hpp>
#include <pst_parser/pst.hpp>
#include <base/variant.hpp>
#include <iostream>
#include <format>

#include <graphviz/gvc.h>

struct Handler {
	GVC_t*    gvc;
	Agraph_t* graph;

	usize id = 0;

	std::string next() {
		std::stringstream ss;
		ss << (id++);
		return ss.str();
	}

	Handler() = delete;

	Handler(std::string s): gvc(gvContext()), graph(agopen(s.data(), Agdirected, 0)) {}

	Agnode_t* addNode(std::string s) {
		auto name = next();
		auto node = agnode(graph, name.data(), 1);
		agsafeset(node, "label", s.data(), "");
		return node;
	}

	Agedge_t* addEdge(Agnode_t* from, Agnode_t* to) { return agedge(graph, from, to, nullptr, 1); }

	void writeToSVG(std::string file_name) {
		FILE* file = fopen(file_name.c_str(), "w");

		gvLayout(gvc, graph, "dot");
		gvRender(gvc, graph, "svg", file);

		fclose(file);
	}

	~Handler() {
		gvFreeLayout(gvc, graph);
		agclose(graph);
	}
};

void handleToken(pst::RiftElement::SubToken token) { std::cerr << " " << token->getStrValue(); }

void handleElement(pst::ParserCBorrowRef<pst::RiftElement> el) {
	for (auto sub: el->viewSubElements()) {
		variant_match(sub) {
			variant_case(pst::RiftElement::SubToken, token) { handleToken(token); }
			variant_case(pst::RiftElement::Child, child) { handleElement(child); }
		}
	}
}

std::string stringPosition(dia::SourcePosition pos) {
	std::stringstream ss;
	ss << "(" << pos.getStart() << ", ";
	ss << pos.getEnd() << ")";
	return ss.str();
}

Agnode_t* dotElement(Handler& hdl, pst::ParserCBorrowRef<pst::RiftElement> el) {
	auto self
		= hdl.addNode(stringPosition(el->getSourcePosition()) + "\n" + typeid(*el.get()).name());
	for (auto sub: el->viewSubElements()) {
		variant_match(sub) {
			variant_case(pst::RiftElement::SubToken, token) {
				std::string value = { token->getStrValue().data(), token->getStrValue().size() };
				auto sub_node = hdl.addNode(stringPosition(token->getPosition()) + "\n" + value);
				hdl.addEdge(self, sub_node);
				agsafeset(sub_node, "shape", "box", "");
			}
			variant_case(pst::RiftElement::Child, child) {
				auto sub_node = dotElement(hdl, child);
				hdl.addEdge(self, sub_node);
			}
		}
	}
	return self;
}

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
		Handler hdl("graph");
		dotElement(hdl, pst.getRootElement());
		hdl.writeToSVG(".dot.svg");
	}
}
