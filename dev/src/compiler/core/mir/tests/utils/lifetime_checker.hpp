/**
 * @file lifetime_checker.hpp
 * @brief Helper for validating lifetime sequences in MIR functions
 * It allows to specify the order of the expected lifetime events (constructions, destructions,
 moves) and specific instructions,
 * and it validates the presence and order of these events in the given MIR function.

 * @author Wojciech Rzepliński
 */

#pragma once

#include <mir/mir_structure/mir_structure.hpp>

#include <string>
#include <variant>
#include <vector>
#include <iostream>

namespace compiler::mir::test_utils {

	struct ExpectedFlagEvent {
		OperationFlag::Flag flag;
		std::string         variable_name;
	};

	struct InstructionTypeEvent {
		Operation operation;
	};

	using ExpectedEvent = std::variant<ExpectedFlagEvent, InstructionTypeEvent>;

	/**
	 * @brief Parses and validates lifetime sequences in MIR output
	 *
	 * Usage:
	 * @code
	 * LifetimeChecker validator;
	 * validator.expectFlag(OperationFlag::Flag::Construct, "c")
	 *          .expectFlag(OperationFlag::Flag::Construct, "a")
	 *          .expectFlag(OperationFlag::Flag::Destruct, "a")
	 *          .expectFlag(OperationFlag::Flag::Construct, "b")
	 *          .expectFlag(OperationFlag::Flag::Destruct, "b")
	 *          .expectInstruction(Operation::ReturnValue)
	 *          .validate(mir_func);
	 * @endcode
	 *
	 * It iterates through the blocks in the block order,
	 * and for each instruction (including terminators) it checks if it matches the expected event.
	 * There can be multiple events in one instruction (like 2 "constructs" in one MIR instruction).
	 * Flag events are matched in order within the flags vector of each instruction.
	 */
	class LifetimeChecker {
	public:
		LifetimeChecker& expectFlag(OperationFlag::Flag flag, std::string_view var_name) {
			events.emplace_back(ExpectedFlagEvent{ .flag          = flag,
			                                       .variable_name = std::string(var_name) });
			return *this;
		}

		LifetimeChecker& expectConstruct(std::string_view var_name) {
			return expectFlag(OperationFlag::Flag::Construct, var_name);
		}

		LifetimeChecker& expectDestruct(std::string_view var_name) {
			return expectFlag(OperationFlag::Flag::Destruct, var_name);
		}

		LifetimeChecker& expectScopeStart(std::string_view var_name) {
			return expectFlag(OperationFlag::Flag::ScopeStart, var_name);
		}

		LifetimeChecker& expectScopeEnd(std::string_view var_name) {
			return expectFlag(OperationFlag::Flag::ScopeEnd, var_name);
		}

		LifetimeChecker& expectMove(std::string_view var_name) {
			return expectFlag(OperationFlag::Flag::Move, var_name);
		}

		LifetimeChecker& expectInstruction(Operation op) {
			events.emplace_back(InstructionTypeEvent{ op });
			return *this;
		}

		void validate(CRef<Function> mir_func) {
			usize event_idx = 0;

			// Walk through all blocks in order
			for (const auto& block_id: mir_func->block_order) {
				const auto& block = mir_func->blocks.at(block_id);

				// Check instructions
				for (const auto& instr: block->instructions) {
					if (event_idx >= events.size()) return;  // All events satisfied

					flag_position = 0;  // Reset flag position for new instruction
					while (event_idx < events.size()
					       && matchEvent(instr, events[event_idx], mir_func)) {
						event_idx++;
					}
				}

				// Check terminator
				if (event_idx < events.size()) {
					flag_position = 0;  // Reset flag position for terminator
					while (event_idx < events.size()
					       && matchEvent(block->terminator, events[event_idx], mir_func))
						event_idx++;
				}
			}

			if (event_idx < events.size()) {
				std::cerr << "Not all expected lifetime events were found in MIR. First unmatched "
							 "event index: "
						  << event_idx << ", total expected events: " << events.size() << "\n";
				throw base::LogicError("Not all lifetime events were found in MIR");
			}
		}

	private:
		std::vector<ExpectedEvent> events;
		usize flag_position = 0;  // Track position within current instruction's flags

		[[nodiscard]] bool matchEvent(
			const Instruction& instr, const ExpectedEvent& expected, CRef<Function> mir_func
		) {
			return std::visit(
				[&](const auto& event) { return matchEventImpl(instr, event, mir_func); }, expected
			);
		}

		[[nodiscard]] bool
			matchEventImpl(const Instruction& instr, const ExpectedFlagEvent& event, CRef<Function>) {
			// Search from current flag_position onwards
			for (usize i = flag_position; i < instr.flags.size(); ++i) {
				const auto& flag = instr.flags[i];
				if (flag.flag == event.flag) {
					auto local_name = flag.local->getName();
					if (local_name.strView() == event.variable_name) {
						flag_position = i + 1;  // Update position to after this match
						return true;
					}
				}
			}
			return false;
		}

		[[nodiscard]] bool
			matchEventImpl(const Instruction& instr, const InstructionTypeEvent& event, CRef<Function>)
				const {
			return instr.operation == event.operation;
		}
	};

}  // namespace compiler::mir::test_utils
