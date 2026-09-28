#include "doc.hpp"

#include "source_text.hpp"

namespace formatter::doc {
	namespace {

		u32 saturatingAdd(u32 a, u32 b) {
			return (a > WIDTH_INFINITE - b) ? WIDTH_INFINITE : a + b;
		}

		/** Sums children widths and ORs their must_break flags into @p node. */
		Doc withCachedLayout(Doc node) {
			u32 width = 0;
			for (const auto& child: node.children) {
				width = saturatingAdd(width, child.flat_width);
				if (child.must_break) node.must_break = true;
			}
			node.flat_width = node.must_break ? WIDTH_INFINITE : width;
			return node;
		}

		Doc make(DocKind kind) {
			Doc node;
			node.kind = kind;
			return node;
		}
	}

	Doc text(std::string content) {
		Doc node  = make(DocKind::Text);
		node.text = std::move(content);
		// A multi-line block comment cannot render on one line, so no enclosing group can.
		node.must_break = node.text.contains('\n');
		node.flat_width = node.must_break ? WIDTH_INFINITE : visualWidth(node.text);
		return node;
	}

	Doc space() { return text(" "); }

	Doc concat(std::vector<Doc> children) {
		Doc node      = make(DocKind::Concat);
		node.children = std::move(children);
		return withCachedLayout(std::move(node));
	}

	Doc indent(std::vector<Doc> children) {
		Doc node      = make(DocKind::Indent);
		node.children = std::move(children);
		return withCachedLayout(std::move(node));
	}

	Doc line(u32 blanks) {
		Doc node        = make(DocKind::Line);
		node.blanks     = blanks;
		node.spaced     = true;
		node.flat_width = 1;
		return node;
	}

	Doc softLine() { return make(DocKind::Line); }

	Doc group(bool breakable, std::vector<Doc> children) {
		Doc node       = make(DocKind::Group);
		node.children  = std::move(children);
		node.breakable = breakable;
		return withCachedLayout(std::move(node));
	}

	Doc fill(bool spaced, std::vector<Doc> children) {
		Doc node      = make(DocKind::Fill);
		node.children = std::move(children);
		node.spaced   = spaced;
		node          = withCachedLayout(std::move(node));
		if (spaced && node.children.size() > 1 && !node.must_break)
			node.flat_width
				= saturatingAdd(node.flat_width, static_cast<u32>(node.children.size()) - 1);
		return node;
	}

	Doc lineComment(std::string content) {
		Doc node        = make(DocKind::LineComment);
		node.text       = std::move(content);
		node.flat_width = WIDTH_INFINITE;
		node.must_break = true;
		return node;
	}
}
