/**
 * @file lifetime_validator.hpp
 * @brief Helper for validating lifetime sequences in MIR functions
 * It allows to specify the order of the expected lifetime events (constructions, destructions, moves) and specific instructions,
 * and it validates the presence and order of these events in the given MIR function.

 * @author Wojciech Rzepliński
 */

#pragma once

#include <mir/mir_structure/mir_structure.hpp>

#include <string>
#include <vector>
#include <variant>

namespace compiler::mir::test_utils {

	enum class LifetimeEventType {
		Construct,
		Destruct,
        Move
	};

	struct LifetimeEvent {
		LifetimeEventType type;
		std::string       variable_name;
	};

	struct InstructionTypeEvent {
		Operation operation;
	};

	using ExpectedEvent = std::variant<LifetimeEvent, InstructionTypeEvent>;

	/**
	 * @brief Parses and validates lifetime sequences in MIR output
	 *
	 * Usage:
	 * @code
	 * LifetimeValidator validator;
	 * validator.expectConstruct("c")
	 *          .expectConstruct("a")
	 *          .expectDestruct("a")
	 *          .expectConstruct("b")
	 *          .expectDestruct("b")
	 *          .expectInstruction(Operation::ReturnValue)
	 *          .validate(mir_func);
	 * @endcode
     *
     * It iterates through the blocks in the block order,
     * and for each instruction (including terminators) it checks if it matches the expected event.
     * There can be multiple events in one instruction (like 2 "constructs" in one MIR instruction).
	 */
	class LifetimeValidator {
	public:
		LifetimeValidator& expectConstruct(std::string_view var_name) {
			events.emplace_back(LifetimeEvent{ .type=LifetimeEventType::Construct, .variable_name=std::string(var_name) });
			return *this;
		}

		LifetimeValidator& expectDestruct(std::string_view var_name) {
			events.emplace_back(LifetimeEvent{ .type=LifetimeEventType::Destruct, .variable_name=std::string(var_name) });
			return *this;
		}

		LifetimeValidator& expectInstruction(Operation op) {
			events.emplace_back(InstructionTypeEvent{ op });
			return *this;
		}

		void validate(const Function& mir_func) {
			usize event_idx = 0;

			// Walk through all blocks in order
			for (const auto& block_id: mir_func.block_order) {
				const auto& block = mir_func.blocks.at(block_id);

				// Check instructions
				for (const auto& instr: block->instructions) {
					if (event_idx >= events.size()) return;  // All events satisfied

					while (matchEvent(instr, events[event_idx], mir_func)) {
						event_idx++;
					}
				}

				// Check terminator
				if (event_idx < events.size()) {
					while (matchEvent(block->terminator, events[event_idx], mir_func)) {
						event_idx++;
					}
				}
			}

			if (event_idx < events.size()) {
				throw base::LogicError("Not all lifetime events were found in MIR");
			}
		}

	private:
		std::vector<ExpectedEvent> events;

		[[nodiscard]] bool matchEvent(const Instruction& instr, const ExpectedEvent& expected, const Function& mir_func) const {
			return std::visit(
				[&](const auto& event) { return matchEventImpl(instr, event, mir_func); },
				expected
			);
		}

		[[nodiscard]] bool matchEventImpl(const Instruction& instr, const LifetimeEvent& event, const Function&) const {
			if (event.type == LifetimeEventType::Construct) {
				for (const auto& flag: instr.flags) {
					if (flag.flag == OperationFlag::Flag::Construct) {
						auto local_name = flag.local->getName();
						if (local_name.strView() == event.variable_name) {
							return true;
						}
					}
				}
			} else if (event.type == LifetimeEventType::Destruct) {
				for (const auto& flag: instr.flags) {
					if (flag.flag == OperationFlag::Flag::Destruct) {
						auto local_name = flag.local->getName();
						if (local_name.strView() == event.variable_name) {
							return true;
						}
					}
				}
			} else if (event.type == LifetimeEventType::Move) {
				for (const auto& flag: instr.flags) {
					if (flag.flag == OperationFlag::Flag::Move) {
						auto local_name = flag.local->getName();
						if (local_name.strView() == event.variable_name) {
							return true;
						}
					}
				}
			}
			return false;
		}

		[[nodiscard]] bool matchEventImpl(const Instruction& instr, const InstructionTypeEvent& event, const Function&) const {
			return instr.operation == event.operation;
		}
	};

}  // namespace compiler::mir::test_utils
