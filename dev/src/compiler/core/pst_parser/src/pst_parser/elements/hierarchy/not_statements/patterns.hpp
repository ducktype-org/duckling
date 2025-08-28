/**
 * @brief File representing all patterns available in the match expression.
 * @note All the patterns are in a single file, since FlowPattern -> AnalysisPattern -> TuplePattern
 * create a cycle and need full type definitions.
 */
#pragma once

#include "../lists/flow_pattern_list.hpp"
#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Represents a flow pattern.
	 */
	class FlowPattern final: public NotStmt {
		AccessInternal<AnalysisPattern>                     pattern;
		base::Optional<tpc::Identifier>                     as_identifier;
		base::Optional<AccessInternal<UniversalExprHolder>> type_constraint;

	public:
		explicit FlowPattern(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::FlowPattern;
		}

		~FlowPattern() final = default;

		[[nodiscard]] AccessLocked<AnalysisPattern> getPattern() const { return pattern.give(); }

		[[nodiscard]] base::Optional<base::StrID> getAsIdentifier() const {
			return as_identifier->value;
		}

		[[nodiscard]] base::Optional<AccessLocked<UniversalExprHolder>> getTypeConstraint() const {
			return type_constraint.map([](const auto& value) { return value.give(); });
		}

		static MBox<FlowPattern> parse(LangParserState& state);
		void                     dprint(std::ostream& out) const final;

		[[nodiscard]] std::string elementType() const override { return "Flow Pattern"; }

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Base class for analysis patterns.
	 */
	class AnalysisPattern: public NotStmt {
	public:
		explicit AnalysisPattern(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::AnalysisPattern;
		}

		virtual ~AnalysisPattern() = default;

		static MBox<AnalysisPattern> parse(LangParserState& state);

		[[nodiscard]]
		std::string elementType() const override {
			return "Analysis Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Represents an deconstructor pattern.
	 */
	class DeconstructorPattern final: public AnalysisPattern {
		tpc::Identifier                 deconstructor_name;
		AccessInternal<FlowPatternList> arguments;

	public:
		explicit DeconstructorPattern(const dia::SourcePosition& position):
			  AnalysisPattern(position) {
			this->element_kind = ElementKind::DeconstructorPattern;
		}

		~DeconstructorPattern() final = default;

		static MBox<DeconstructorPattern> parse(LangParserState& state);
		void                              dprint(std::ostream& out) const final;

		[[nodiscard]] base::StrID getDeconstructorName() const { return deconstructor_name.value; }

		[[nodiscard]] const AccessLocked<FlowPatternList> getArguments() const {
			return arguments.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Deconstructor Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Represents an Tuple pattern, which contains a list of flow patterns.
	 */
	class TuplePattern final: public AnalysisPattern {
		AccessInternal<FlowPatternList> elements;

	public:
		explicit TuplePattern(const dia::SourcePosition& position): AnalysisPattern(position) {
			this->element_kind = ElementKind::TuplePattern;
		}

		static MBox<TuplePattern> parse(LangParserState& state);
		void                      dprint(std::ostream& out) const final;
		~TuplePattern() final = default;

		[[nodiscard]] const AccessLocked<FlowPatternList> getElements() const {
			return elements.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Tuple Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Represents the '_' wildcard pattern.
	 */

	class WildcardPattern final: public AnalysisPattern {
	public:
		explicit WildcardPattern(const dia::SourcePosition& position): AnalysisPattern(position) {
			this->element_kind = ElementKind::WildcardPattern;
		}

		static MBox<WildcardPattern> parse(LangParserState& state);
		void                         dprint(std::ostream& out) const final;
		~WildcardPattern() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Wildcard Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Represents an binding pattern, which binds a value to a new variable.
	 */
	class BindingPattern final: public AnalysisPattern {
		tpc::Identifier name;

	public:
		explicit BindingPattern(const dia::SourcePosition& position): AnalysisPattern(position) {
			this->element_kind = ElementKind::BindingPattern;
		}

		~BindingPattern() final = default;

		static MBox<BindingPattern> parse(LangParserState& state);
		void                        dprint(std::ostream& out) const final;

		[[nodiscard]] base::StrID getName() const { return name.value; }

		[[nodiscard]]
		std::string elementType() const override {
			return "Binding Pattern";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Represents a pattern which is an expression interpreted as a value. Either a literal,
	 * block expression or an identifier.
	 */
	class ValuePattern final: public AnalysisPattern {
		// TODOP: Universal? What with CodeBlocks with no semicolon like this {my_var}.
		AccessInternal<UniversalExprHolder> expression;

	public:
		explicit ValuePattern(const dia::SourcePosition& position): AnalysisPattern(position) {
			this->element_kind = ElementKind::ValuePattern;
		}

		static MBox<ValuePattern> parse(LangParserState& state);
		void                      dprint(std::ostream& out) const final;
		~ValuePattern() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Value Pattern";
		}

		[[nodiscard]] base::Optional<AccessLocked<UniversalExprHolder>> getExpression() const {
			return expression.give();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
