#pragma once
#include "diagnostic_file.hpp"
#include "template_file.hpp"
#include "diagnostic_state.hpp"
#include "template_registry.hpp"

#include "base/maps.hpp"
#include <base/box.hpp>

namespace dia_app {

	// Forward declarations
	class ConstructTextViewVisitor;
	class EvaluateTemplateFileVisitor;
	class EvaluateDiagnosticFileVisitor;

	state::Message evaluateMessage(
		const template_file::MessageTemplate&                message_template,
		const dia_file::Message&                             message,
		TemplateRegistry&                                    registry,
		const base::HashMap<std::string, dia_file::Message>* attached_messages = nullptr
	);

	state::Diagnostic evaluateDiagnostic(const dia_file::Thread& thread);

	std::string constructTextView(CRef<state::Component> component);

	
}
