#include "access.hpp"
#include "lang_parser_element.hpp"
#include "lang_parser_state.hpp"

#include <base/exceptions.hpp>
#include <base/str_utils.hpp>
#include <base/variant.hpp>

#include <ranges>

namespace pst {
	std::string ElementPath::str() {
		using namespace std::ranges;
		using namespace std::views;
		return elements | join_with('.') | to<std::string>();
	}

	base::Optional<AccessLocked<LangElement>> LangElement::getParent() const { return parent; }

	void LangParserState::addImport(const ImportType& import) { imports.push_back(import); }

	const dia::SourcePosition& LangElement::getSourcePosition() const { return source_position; }

	void LangElement::calcElementPathsRecursive(const ElementPath& path) {
		for(auto& el: sub_elements) {
			variant_match(el) {
				variant_case(InternalChild, child) {
					CORE_PANIC("Default implementation of calculating element paths cannot handle unnamed sub-elements. Encountered while calculating for: " + elementType());
				}
				variant_case(InternalNamedChild, named_child) {
					ElementPath child_path(path, named_child.first);
					named_child.second->calcElementPaths(child_path);
				}
			}
		}
	}

	void LangElement::addToken(CRef<tpc::Token> t) {
		sub_elements.emplace_back(t);
		setLastToken(t->getPosition());
	}

	void LangElement::addToken(const Box<tpc::Token>& t) { addToken(t.ref()); }

	void LangElement::addToken(const tpc::Token& t) { addToken(&t); }

	void LangElement::addChild(Ref<LangElement> el) {
		sub_elements.emplace_back(el );
		setLastToken(el->getSourcePosition());
	}

	void LangElement::addChild(MRef<LangElement> el) {
		auto opt = el.toOpt();
		if (opt) {
			addChild(opt.value());
		}
	}

	void LangElement::addNamedChild(const std::string& name, Ref<LangElement> el) {
		sub_elements.emplace_back(InternalNamedChild{name, el});
		setLastToken(el->getSourcePosition());
	}

	void LangElement::addNamedChild(const std::string& name, MRef<LangElement> el) {
		auto opt = el.toOpt();
		if (opt) {
			addNamedChild(name, opt.value());
		}
	}

	void LangElement::setFirstToken(dia::SourcePosition pos) {
		if (pos.getStart() < source_position.getStart())
			source_position = dia::SourcePosition(pos, source_position.getEnd());
	}

	void LangElement::setLastToken(dia::SourcePosition pos) {
		if (not pos.isFileEnd() && pos.getEnd() > source_position.getEnd())
			source_position = dia::SourcePosition(source_position, pos.getEnd());
	}

	void LangElement::acceptVisitor(PstVisitor&) const {
		CORE_PANIC("PstVisitor not supported for " + elementType());
	}

	base::HashMap<u64, AccessLocked<LangElement>> LangElement::pst_id_map{};

	AccessLocked<LangElement> LangElement::getByID(u64 id) { return pst_id_map.at(id); }
}
