#pragma once

#include <json/json.hpp>
#include <base/box.hpp>
#include <helios/hout/elements/expr.hpp>
#include "base/string_id.hpp"
#include "serializable.hpp"
#include "interactive_content.hpp"
#include <vector>
#include <concepts>

namespace dia {
	using nlohmann::json;

	class InteractiveMessage {
	private:
		Box<dia::InteractiveContent>           content;
		std::vector<Box<dia::InteractiveNote>> notes;

	public:
		InteractiveMessage(
			Box<dia::InteractiveContent> content, std::vector<Box<dia::InteractiveNote>>&& notes
		):
			  content(std::move(content)),
			  notes(std::forward<decltype(notes)>(notes)) {}

		friend void to_json(json& j, const InteractiveMessage& message) { j = message.tojson(); }

		json tojson() const {
			auto content_symbols = content->get_symbols();
			for (auto& note: notes) {
				auto note_symbols = note->get_symbols();
				content_symbols.insert(note_symbols.begin(), note_symbols.end());
			}
			auto content_types = content->get_types();
			for (auto& note: notes) {
				auto note_types = note->get_types();
				content_types.insert(note_types.begin(), note_types.end());
			}
			return json{ { "content", content },
				         { "notes", notes },
				         { "symbols", content_symbols },
				         { "types", content_types } };
		}
	};

	class ExampleMessage: public InteractiveMessage {
	public:
		ExampleMessage():
			  InteractiveMessage(
				  base::makeBox<ExampleContent>(), std::vector<Box<dia::InteractiveNote>>()
			  ) {}
	};

	using compiler::helios::code::Expr;

	class OperatorNotFound: public InteractiveMessage {
		class Params: public ContentParams {
		public:
			base::StrID op;
			Box<Expr>   lhs;
			Box<Expr>   rhs;

			Params(base::StrID op, Box<Expr> lhs, Box<Expr> rhs):
				  ContentParams(),
				  op(op),
				  lhs(std::move(lhs)),
				  rhs(std::move(rhs)) {
				auto& lhs_type = this->lhs->expression_type;
				auto& rhs_type = this->rhs->expression_type;
				types.insert(lhs_type.getType());
				types.insert(rhs_type.getType());
			}

			json tojson() override {
				json lhs_type, rhs_type;
				lhs_type["type"]
					= std::to_string(lhs->expression_type.getType().customPerfectHash());
				rhs_type["type"]
					= std::to_string(rhs->expression_type.getType().customPerfectHash());
				return json{ { "operator", op.strView() },
					         { "left_type", lhs_type },
					         { "right_type", rhs_type } };
			}
		};

		class Content: public InteractiveContent {
		public:
			Content(Box<ContentParams> params, dia::SourcePosition position):
				  InteractiveContent(ContentType::ERROR, 1'001, std::move(params), position) {}
		};

	public:
		OperatorNotFound(
			dia::SourcePosition position, base::StrID op, Box<Expr> lhs, Box<Expr> rhs
		):
			  InteractiveMessage(
				  base::makeBox<Content>(
					  base::makeBox<Params>(op, std::move(lhs), std::move(rhs)), position
				  ),
				  std::vector<Box<dia::InteractiveNote>>{}
			  ) {}
	};

}
