#include "lifecycle.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace vm::lifecycle {
	namespace {
		Definition buildDefinition() {
			using api::ExecutionCompleted;
			using api::ExecutionPanicked;
			using api::ExecutionStopped;
			using api::NotStarted;
			using api::Paused;
			using api::ProcStatus;
			using api::Running;
			using api::Sleeping;

			Definition def;

			// Terminal source states cover process / main thread reuse, e.g.
			// consecutive `runFunctionAwait` calls on the same process.
			def.addTransitions<Start, NotStarted, ExecutionCompleted, ExecutionStopped, ExecutionPanicked>(
				[](const ProcStatus&, const Start&) -> ProcStatus { return Running{}; }
			);

			// `Paused` source: a single step re-announces `Paused` after it finishes.
			def.addTransitions<Pause, Running, Paused>(
				[](const ProcStatus&, const Pause&) -> ProcStatus { return Paused{}; }
			);

			def.addTransition<Paused, Resume>([](const Paused&, const Resume&) -> ProcStatus {
				return Running{};
			});

			def.addTransition<Running, Sleep>([](const Running&, const Sleep&) -> ProcStatus {
				return Sleeping{};
			});

			def.addTransition<Sleeping, Wake>([](const Sleeping&, const Wake&) -> ProcStatus {
				return Running{};
			});

			// `Paused` source: a single step can execute the last instruction of the
			// program. Terminal source: re-announcement of an inherited terminal status.
			def.addTransitions<Complete, Running, Paused, ExecutionCompleted>(
				[](const ProcStatus&, const Complete& event) -> ProcStatus {
					return ExecutionCompleted{ event.exit_value };
				}
			);

			def.addTransitions<Stop, Running, Paused, Sleeping, ExecutionStopped>(
				[](const ProcStatus&, const Stop&) -> ProcStatus { return ExecutionStopped{}; }
			);

			def.addTransitions<Panic, Running, Paused, Sleeping, ExecutionPanicked>(
				[](const ProcStatus&, const Panic& event) -> ProcStatus {
					return ExecutionPanicked{ event.error_message };
				}
			);

			// `NotStarted` source keeps a double reset (e.g. join right after a join
			// that already recycled the thread) a harmless no-op.
			def.addTransitions<Reset, NotStarted, ExecutionCompleted, ExecutionStopped, ExecutionPanicked>(
				[](const ProcStatus&, const Reset&) -> ProcStatus { return NotStarted{}; }
			);

			return def;
		}
	}

	const Definition& statusTransitions() {
		// Intentionally leaked: VM threads can still dispatch lifecycle events during
		// static destruction (e.g. the Supervisor stopping leftover processes in its
		// destructor), so the table must outlive every other static object.
		static const Definition* def = new Definition(buildDefinition());  // NOLINT
		return *def;
	}

	Event eventForTerminalStatus(const api::ProcStatus& status) {
		variant_match(status) {
			variant_case(api::ExecutionCompleted, completed) {
				return Event{ Complete{ completed.exit_value } };
			}
			variant_case_novalue(api::ExecutionStopped) { return Event{ Stop{} }; }
			variant_case(api::ExecutionPanicked, panicked) {
				return Event{ Panic{ panicked.error_message } };
			}
			variant_default {
				CORE_PANIC("eventForTerminalStatus called with a non-terminal status");
			}
		}
		CORE_UNREACHABLE();
	}
}
