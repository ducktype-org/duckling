#include "diagnostic_state.hpp"

namespace dia_app {

	/**
	 * @brief Convert state components to plain text.
	 * Used for evaluating patterns in CaseOfComponent.
	 */
	class ConstructTextViewVisitor: public state::ComponentVisitorPanicky {
	public:
		std::string result;

		void visitTextComponent(const state::TextComponent& el) override { result += el.content; }

		void visitCodeComponent(const state::CodeComponent& el) override { result += el.content; }

		void visitConcatComponent(const state::ConcatComponent& el) override {
			for (const auto& component: el.components) (*component).acceptVisitor(*this);
		}

		void visitInteractiveComponent(const state::InteractiveComponent& el) override {
			(*el.primary).acceptVisitor(*this);
		}

		void visitStartLineComponent(const state::StartLineComponent&) override { result += "\n"; }

		void visitCodeBlockComponent(const state::CodeBlockComponent& el) override {
			(*el.content).acceptVisitor(*this);
		}

		void visitCodeLocationComponent(const state::CodeLocationComponent&) override {}
	};

	std::string constructTextView(CRef<state::Component> component) {
		ConstructTextViewVisitor text_visitor;
		component->acceptVisitor(text_visitor);
		return text_visitor.result;
	}
}
