#include "debugger.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <filesystem/file_path.hpp>

#include <vm/api/vm.hpp>

namespace {
	inline std::string statusToString(const vm::api::ProcStatus& status) {
		return std::visit(
			[](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				return TypeParseTraits<T>::NAME.data();
			},
			status
		);
	}
}

namespace vm::debugger {
	Debugger::Debugger():
		  updater([&](const api::ProcStatus& status) {
			  on_status_changed.emitEvent(status);
			  variant_match(status) {
				  variant_case(api::ExecutionPanicked, panicked) {
					  on_error.emitEvent(panicked.error_message);
				  }
			  }
		  }),
		  vm_output([&](const std::string& str) { on_output.emitEvent(str); }) {
		api::spawn()
			.and_then([&](const api::ProcessInfo& info) {
				pid = info.pid;
				return api::attachStatusListener(pid, &updater);
			})
			.and_then([&] { return api::attachOutputListener(pid, &vm_output); })
			.transform_error([&](const api::ApiError& api_error) -> std::monostate {
				throw std::runtime_error(api::errorToString(api_error));
			});
	}

	Debugger::~Debugger() {
		updater.detach();

		(void) api::kill(pid).transform_error([&](const api::ApiError& api_error) {
			on_error.emitEvent(api::errorToString(api_error));
			return api_error;
		});
	}

	void Debugger::attachOnStatusChangedListener(events::Listener<api::ProcStatus>& listener) {
		on_status_changed.attachListener(listener);
	}

	void Debugger::attachOnErrorListener(events::Listener<std::string>& listener) {
		on_error.attachListener(listener);
	}

	void Debugger::attachOnOutputListener(events::Listener<std::string>& listener) {
		on_output.attachListener(listener);
	}

	CodePosition Debugger::mapCodePosition(const api::response::CodePosition& pos) {
		CodePosition cp;
		cp.function_name   = pos.function_name;
		cp.instr_number    = pos.instr_number;
		cp.source_position = pos.source_position;
		cp.mapped_position
			= mapper.mapCodePositionToSourcePosition(pos.function_name, pos.instr_number);
		return cp;
	}

	std::expected<void, api::ApiError> Debugger::runMain() {
		return api::getExecutionStatus(pid)
		    .and_then([&](const api::ProcStatus& status) -> std::expected<void, api::ApiError> {
				variant_match(status) {
					variant_case_novalue(api::ExecutionCompleted) { return api::join(pid); }
					variant_case_novalue(api::NotStarted) { return {}; }
					variant_default {
						return std::unexpected(api::ApiError{ api::OtherError{
							"Wrong VM state to run: got " + statusToString(status)
							+ ", allowed states are NotStarted and ExecutionCompleted." } });
					}
				}
			})
		    .and_then([&] { return api::run(pid, main_args); });
	}

	api::ProcStatus Debugger::getStatus() {
		// will never fail when pid is correct
		return api::getExecutionStatus(pid)
		    .transform_error([&](const api::ApiError& api_error) -> std::monostate {
				throw std::runtime_error(api::errorToString(api_error));
			})
		    .value();
	}

	std::expected<void, api::ApiError> Debugger::loadFiles(const std::vector<fs::File>& files) {
		return api::loadFiles(pid, files);
	}

	std::expected<void, std::variant<api::ApiError, std::string>> Debugger::loadDefault(
		base::Optional<fs::FilePath> prefix
	) {
		fs::FilePath fp = "duck_build/package_dvm.dbc";
		if (prefix.has_value()) fp = prefix.value() / fp;
		if (!fp.exists())
			return std::unexpected(api::OtherError{
				"No compiled program in the current directory." });

		fs::FilePath fp_map = "duck_build/package_dvm.di.json";
		if (prefix.has_value()) fp_map = prefix.value() / fp_map;
		if (!fp_map.exists())
			return std::unexpected(api::OtherError{
				"No compiled program mapping in the current directory." });

		auto resp = mapper.loadMapping(fp_map);
		if (!resp) return resp;

		return loadFiles({ fp });
	}

	void Debugger::setProgramArguments(const ProgramRunArguments& args) { main_args = args; }

	std::expected<u64, api::ApiError> Debugger::getNumberOfStackFrames(api::ThreadID thread_id) {
		return api::debuggerGetNumberOfStackFrames(pid, thread_id)
		    .transform([](api::response::NumberOfCurrentStackFrames nosf) {
				return nosf.number_of_stack_frames;
			});
	}

	std::expected<api::response::StackFrameData, api::ApiError> Debugger::getStackFrameData(
		api::ThreadID thread_id, u64 frame_index
	) {
		return api::debuggerGetStackFrameData(pid, thread_id, frame_index);
	}

	std::expected<CodePosition, api::ApiError> Debugger::pause() {
		return api::getExecutionStatus(pid)
		    .and_then(
				[&](const api::ProcStatus& status
		        ) -> std::expected<api::response::CodePosition, api::ApiError> {
					if (std::holds_alternative<api::Running>(status)) return api::pause(pid);
					return std::unexpected(api::ApiError{
						api::OtherError{ "Wrong VM state to pause: got " + statusToString(status)
			                             + ", allowed state is Running." } });
				}
			)
		    .transform(std::bind_front(&Debugger::mapCodePosition, this));
	}

	std::expected<void, api::ApiError> Debugger::resume() {
		return api::getExecutionStatus(pid)
		    .and_then([&](const api::ProcStatus& status) -> std::expected<void, api::ApiError> {
				if (std::holds_alternative<api::Paused>(status)) return {};

				return std::unexpected(api::ApiError{
					api::OtherError{ "Wrong VM state to resume: got " + statusToString(status)
			                         + ", allowed state is Paused." } });
			})
		    .and_then([&] { return api::resume(pid); });
	}

	std::expected<CodePosition, api::ApiError> Debugger::getCurrentPosition(
		base::Optional<usize> frame_idx
	) {
		return api::getCurrentPosition(pid, frame_idx)
		    .transform(std::bind_front(&Debugger::mapCodePosition, this));
	}

	std::expected<void, api::ApiError> Debugger::setBreakpoint(
		base::StrID function_name, u64 instr_number, bool enabled
	) {
		return api::setBreakpoint(pid, function_name, instr_number, enabled);
	}

	std::expected<void, api::ApiError> Debugger::setBreakpoint(
		fs::File file, usize line, bool enabled
	) {
		if_opt_some(mapper.mapSourcePositionToCodePosition(file.getFilePath(), line), pos) {
			return setBreakpoint(pos.first, pos.second, enabled);
		}

		return api::mapFileLineToCodeCollectionPosition(pid, std::move(file), line)
		    .and_then([&](const api::response::CodePosition& pos) {
				return setBreakpoint(pos.function_name, pos.instr_number, enabled);
			});
	}

	std::expected<base::Optional<CodePosition>, api::ApiError> Debugger::step() {
		auto step_response = api::step(pid);
		if (!step_response) return std::unexpected(step_response.error());

		// The step may have ended a program, so we don't report any position.
		if (api::isStatusTerminal(getStatus())) return base::Optional<CodePosition>();

		return getCurrentPosition().transform([](const CodePosition& position) {
			return base::Optional<CodePosition>(position);
		});
	}

	std::expected<base::Optional<CodePosition>, api::ApiError> Debugger::mappedStep() {
		auto response = step();

		for (usize guard = 1'024; response && response->has_value() && guard;
		     response    = step(), guard--) {
			const CodePosition& position = response->value();

			if (!position.mapped_position) return response;
			auto mapped = *position.mapped_position;

			auto unmapped = mapper.mapSourcePositionToCodePosition(
				mapped.getLocation()->getSourceFile().getFilePath(),
				mapped.getStartLineColumn().first
			);

			if (!unmapped) return response;
			if (unmapped->first == position.function_name
			    && unmapped->second == position.instr_number)
				return response;
		}

		return response;
	}

	std::expected<void, api::ApiError> Debugger::sendInput(const std::string& msg) {
		return api::input(pid, msg);
	}

	const Mapper& Debugger::getMapper() { return mapper; }
}
