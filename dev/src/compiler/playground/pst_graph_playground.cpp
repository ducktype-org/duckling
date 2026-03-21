#include <frontend/pst_parser/pst.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <filesystem/file.hpp>
#include <init/init.hpp>

#include <graphviz/gvc.h>

#include <iostream>

// Linting is turned off because the graph api uses c-style pointers for text.
// NOLINTBEGIN(-avoid-c-arrays)

/**
 * @brief Graph handler for gvc graphs
 */
struct Handler final {
	GVC_t*    gvc;
	Agraph_t* graph;

	usize id = 0;

	std::string next() {
		std::stringstream ss;
		ss << (id++);
		return ss.str();
	}

	Handler() = delete;

	Handler(std::string s): gvc(gvContext()), graph(agopen(s.data(), Agdirected, nullptr)) {}

	Agnode_t* addNode(std::string s) {
		auto               name         = next();
		auto               node         = agnode(graph, name.data(), 1);
		static std::string label_string = "label";
		static std::string empty_string = "";
		agsafeset(node, label_string.data(), s.data(), empty_string.data());
		return node;
	}

	Agedge_t* addEdge(Agnode_t* from, Agnode_t* to, char* name = nullptr) {
		Agedge_t*          edge         = agedge(graph, from, to, nullptr, 1);
		static std::string label_string = "label";
		static std::string empty_string = "";
		agsafeset(edge, label_string.data(), name, empty_string.data());
		return edge;
	}

	void writeToSVG(const std::string& file_name) {
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

std::string stringPosition(dia::SourcePosition pos) {
	std::stringstream ss;
	ss << "(" << pos.getStart() << ", ";
	ss << pos.getEnd() << ")";
	return ss.str();
}

/**
 * @brief Generates graph from a pst element and it's children and tokens.
 *
 * @note Adds information about position and element class
 */
Agnode_t* dotElement(Handler& hdl, pst::Access<pst::LangElement> el) {
	// std::string name = stringPosition(el->getSourcePosition().unlock(ctx)) + "\n" +
	// el->elementType() + "\n\""
	// + el->getComponentHash().str() + "\"";
	std::string name
		= stringPosition(el->getSourcePosition().illegalAccess()) + "\n" + el->elementType();
	auto self = hdl.addNode(name);

	for (auto sub: el->viewSubElements()) {
		variant_match(sub) {
			variant_case(pst::LangElement::SubToken, token) {
				std::string value = { token->getStrValue().data(), token->getStrValue().size() };
				auto sub_node = hdl.addNode(stringPosition(token->getPosition()) + "\n" + value);
				hdl.addEdge(self, sub_node);
				static std::string shape_string = "shape";
				static std::string box_string   = "box";
				static std::string empty_string = "";
				agsafeset(sub_node, shape_string.data(), box_string.data(), empty_string.data());
			}

			variant_case(pst::LangElement::Child, child) {
				auto sub_node = dotElement(hdl, child.illegalAccess().value());
				hdl.addEdge(self, sub_node);
			}

			variant_case(pst::LangElement::NamedChild, named_child) {
				std::string child_name = named_child.name;
				auto        sub_node = dotElement(hdl, named_child.element.illegalAccess().value());
				hdl.addEdge(self, sub_node, child_name.data());
			}
		}
	}
	return self;
}

// NOLINTEND(-avoid-c-arrays)

int main(int argc, char** argv) {
	init::InitObject _;
	if (argc != 3) {
		std::cerr << "usage: ./pst_graph duckling_file svg_out_file\n";
		return 1;
	}
	fs::File file(argv[1]);
	pst::PST pst(file, pst::PSTType::Program);

	if (pst.getLogger()->bad()) {
		pst.getLogger()->dumpLog(false, std::cerr);
		std::cerr << "\nThere are errors.\n";
		pst.dprint(std::cerr);
		std::cerr << "\n";
	}
	if (pst.getRootElement().illegalAccess()) {
		Handler hdl("graph");
		dotElement(hdl, pst.getRootElement().illegalAccess().value());
		hdl.writeToSVG(argv[2]);
	}
}
