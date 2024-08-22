#pragma once

#include "statements.hpp"

namespace pst {
	class Return final: public Action {
	public:
		explicit Return(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Return() final = default;

		void semPrint(std::ostream& out) const final;
		~Return() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Break final: public Action {
	public:
		explicit Break(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Break() final = default;

		void semPrint(std::ostream& out) const final;
		~Break() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Continue final: public Action {
	public:
		explicit Continue(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Continue() final = default;

		void semPrint(std::ostream& out) const final;
		~Continue() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Redo final: public Action {
	public:
		explicit Redo(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Redo() final = default;

		void semPrint(std::ostream& out) const final;
		~Redo() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Restart final: public Action {
	public:
		explicit Restart(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Restart() final = default;

		void semPrint(std::ostream& out) const final;
		~Restart() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Defer final: public Action {
	public:
		explicit Defer(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Defer() final = default;

		void semPrint(std::ostream& out) const final;
		~Defer() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	/**
	 * @todo Should throw be an action?
	 */
	class Throw final: public Action {
	public:
		explicit Throw(const dia::SourcePosition& position): Action(position) {}

		void dprint(std::ostream& out) const final;
		~Throw() final = default;

		void semPrint(std::ostream& out) const final;
		~Throw() final = default;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

}
