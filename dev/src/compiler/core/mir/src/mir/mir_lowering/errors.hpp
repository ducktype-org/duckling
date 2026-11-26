// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace compiler::mir {
	// Used for reporting unused shadowed variables.
	// Shadowing of used variables gets detected earlier, in HELIOS.
	class VariableShadowingError: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "lookup",
				     .name          = "variable_shadowing" };
		}

	public:
		VariableShadowingError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ShadowedDeclarationNote final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "lookup",
				     .name          = "shadowed_declaration" };
		}

	public:
		ShadowedDeclarationNote(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class NoEnclosingLoopError final: public dia::MessageWithCodeFragmentAndCause {
		[[nodiscard]]
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "no_enclosing_loop" };
		}

	public:
		NoEnclosingLoopError(dia::StablePosition position, std::string_view action):
			  MessageWithCodeFragmentAndCause(position) {
			addArgument<dia::TextArgument>("action", std::string(action));
		}
	};

	class NoNamedControlFlowTargetError final: public dia::MessageWithCodeFragmentAndCause {
		[[nodiscard]]
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "no_named_control_flow_target" };
		}

	public:
		NoNamedControlFlowTargetError(
			dia::StablePosition position, std::string_view action, std::string_view name
		):
			  MessageWithCodeFragmentAndCause(position) {
			addArgument<dia::TextArgument>("action", std::string(action));
			addArgument<dia::TextArgument>("name", std::string(name));
		}
	};

	class NoControlFlowKindTargetError final: public dia::MessageWithCodeFragmentAndCause {
		[[nodiscard]]
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "no_control_flow_kind_target" };
		}

	public:
		NoControlFlowKindTargetError(
			dia::StablePosition position, std::string_view action, std::string_view kind
		):
			  MessageWithCodeFragmentAndCause(position) {
			addArgument<dia::TextArgument>("action", std::string(action));
			addArgument<dia::TextArgument>("kind", std::string(kind));
		}
	};

	class ReturnTypeMismatchError final: public dia::MessageWithCodeFragmentAndCause {
		[[nodiscard]]
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "return_type_mismatch" };
		}

	public:
		ReturnTypeMismatchError(
			dia::StablePosition position, std::string given_type, std::string expected_type
		):
			  MessageWithCodeFragmentAndCause(position) {
			addArgument<dia::TextArgument>("given_type", std::move(given_type));
			addArgument<dia::TextArgument>("expected_type", std::move(expected_type));
		}
	};
}
