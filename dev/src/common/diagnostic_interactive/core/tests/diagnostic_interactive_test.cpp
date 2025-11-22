#include <diagnostic_interactive/core/diagnostic_file.hpp>
#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <diagnostic_interactive/core/template_file.hpp>
#include <yaml-cpp/yaml.h>

#include <tester/tester.hpp>
#include "diagnostic_interactive/core/thread_state.hpp"

#include <json/json.hpp>

using namespace dia_app;

class DiagnosticInteractiveTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DiagnosticInteractiveTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// JSON serialization/deserialization tests
		TESTER_ADD_TEST(jsonComponentsRoundTrip);
		TESTER_ADD_TEST(complexComponentJsonRoundTrip);

		// JSON string round trip tests
		TESTER_ADD_TEST(jsonStringRoundTrip);

		// YAML template deserialization tests
		TESTER_ADD_TEST(yamlTemplateDeserialization);

		// Template evaluation tests
		TESTER_ADD_TEST(evaluateSimpleTextTemplate);
		TESTER_ADD_TEST(evaluateParamTemplate);
		TESTER_ADD_TEST(evaluateConcatTemplate);
		TESTER_ADD_TEST(evaluateCaseOfTemplate);
	}

private:
	// ========================================
	// JSON Serialization/Deserialization Tests
	// ========================================

	void jsonComponentsRoundTrip() {
		// Helper lambda to test component round-trip
		auto test_round_trip
			= [this](Box<dia_file::Component> component, const std::string& expected_type) {
				  // Serialize to JSON
				  json j = component->toJson();

				  // Verify type field
				  ASSERT_EQUAL(expected_type, j["type"].get<std::string>());

				  // Deserialize back
				  auto deserialized = dia_file::Component::fromJson(j);

				  // Verify deserialized type matches
				  ASSERT_EQUAL(expected_type, deserialized->toJson()["type"].get<std::string>());
			  };

		// Test TextComponent
		test_round_trip(base::makeBox<dia_file::TextComponent>("Hello, World!"), "text");

		// Test CodeComponent
		test_round_trip(base::makeBox<dia_file::CodeComponent>("int x = 42;"), "code");

		// Test ConcatComponent
		{
			std::vector<Box<dia_file::Component>> elements;
			elements.push_back(base::makeBox<dia_file::TextComponent>("Error: "));
			elements.push_back(base::makeBox<dia_file::CodeComponent>("variable"));
			test_round_trip(base::makeBox<dia_file::ConcatComponent>(std::move(elements)), "concat");
		}

		// Test EntityComponent
		{
			auto content = base::makeBox<dia_file::CodeComponent>("MyType");
			test_round_trip(
				base::makeBox<dia_file::EntityComponent>(std::move(content), "type_MyType_123"),
				"entity"
			);
		}

		// Test PointedComponent
		{
			auto content = base::makeBox<dia_file::CodeComponent>("x + y");
			std::vector<dia_file::PointerMessage> pointer_messages;
			pointer_messages.push_back(dia_file::PointerMessage(
				"error_location", base::Optional<dia_file::MessageID>("msg_1")
			));
			pointer_messages.push_back(
				dia_file::PointerMessage("hint_location", base::Optional<dia_file::MessageID>())
			);
			test_round_trip(
				base::makeBox<dia_file::PointedComponent>(
					std::move(content), std::move(pointer_messages)
				),
				"pointed"
			);
		}
	}

	void complexComponentJsonRoundTrip() {
		// Create a complex nested structure
		std::vector<Box<dia_file::Component>> inner_elements;
		inner_elements.push_back(base::makeBox<dia_file::TextComponent>("Type "));

		auto entity_content = base::makeBox<dia_file::CodeComponent>("i32");
		inner_elements.push_back(
			base::makeBox<dia_file::EntityComponent>(std::move(entity_content), "builtin_type_i32")
		);

		auto inner_concat = base::makeBox<dia_file::ConcatComponent>(std::move(inner_elements));

		std::vector<Box<dia_file::Component>> outer_elements;
		outer_elements.push_back(std::move(inner_concat));
		outer_elements.push_back(
			base::makeBox<dia_file::TextComponent>(" is a signed 32-bit integer")
		);

		auto original = base::makeBox<dia_file::ConcatComponent>(std::move(outer_elements));

		// Serialize to JSON
		json j = original->toJson();

		// Deserialize back
		auto deserialized = dia_file::Component::fromJson(j);

		// Verify structure
		auto* outer_concat = dynamic_cast<dia_file::ConcatComponent*>(deserialized.get());
		ASSERT_EQUAL(false, outer_concat == nullptr);
		ASSERT_EQUAL(2, outer_concat->elements.size());

		auto* inner_concat_deser
			= dynamic_cast<dia_file::ConcatComponent*>(outer_concat->elements[0].get());
		ASSERT_EQUAL(false, inner_concat_deser == nullptr);
		ASSERT_EQUAL(2, inner_concat_deser->elements.size());
	}

	// ========================================
	// JSON String Round Trip Tests
	// ========================================

	void jsonStringRoundTrip() {
		// Helper lambda to test component JSON string round-trip
		auto test_component_round_trip
			= [this](const std::string& json_str, const std::string& expected_type) {
				  // Parse and deserialize
				  json j         = json::parse(json_str);
				  auto component = dia_file::Component::fromJson(j);

				  // Serialize back to JSON
				  json j2 = component->toJson();

				  // Verify type field matches
				  ASSERT_EQUAL(expected_type, j2["type"].get<std::string>());
			  };

		// Helper lambda to test non-component structure JSON round-trip
		auto test_from_json_to_json_equality
			= [this](const std::string& json_str, auto from_json_func) {
				  // Parse and deserialize
				  json j         = json::parse(json_str);
				  auto structure = from_json_func(j);

				  // Serialize back to JSON
				  json j2 = structure.toJson();

				  ASSERT_EQUAL(j.dump(2), j2.dump(2));
			  };

		// ===== Component Tests =====

		// Test TextComponent
		test_component_round_trip(R"({"type": "text", "content": "Hello from JSON"})", "text");

		// Test CodeComponent
		test_component_round_trip(R"({"type": "code", "content": "int x = 42;"})", "code");

		// Test CodeWithLocationComponent
		test_component_round_trip(
			R"({
			"type": "code_with_location",
			"file": "main.cpp",
			"line": 10,
			"column": 5,
			"content": {"type": "code", "content": "x + y"}
		})",
			"code_with_location"
		);

		// Test StartLineComponent with number
		test_component_round_trip(R"({"type": "start_line", "number": 42})", "start_line");

		// Test StartLineComponent without number
		test_component_round_trip(R"({"type": "start_line"})", "start_line");

		// Test ConcatComponent
		test_component_round_trip(
			R"({
			"type": "concat",
			"content": [
				{"type": "text", "content": "Error: "},
				{"type": "code", "content": "variable"},
				{"type": "text", "content": " not found"}
			]
		})",
			"concat"
		);

		// Test PointedComponent
		test_component_round_trip(
			R"({
			"type": "pointed",
			"content": {"type": "code", "content": "x + y"},
			"pointer_messages": [
				{"pointer_message_id": "error_loc", "message_id": "msg_1"},
				{"pointer_message_id": "hint_loc"}
			]
		})",
			"pointed"
		);

		// Test VariantComponent
		test_component_round_trip(
			R"({
			"type": "variant",
			"content": {"type": "text", "content": "primary"},
			"alt_content": {"type": "text", "content": "alternative"}
		})",
			"variant"
		);

		// Test EntityComponent
		test_component_round_trip(
			R"({
			"type": "entity",
			"content": {"type": "code", "content": "MyType"},
			"refers_to": "type_MyType_123"
		})",
			"entity"
		);

		// Test EvaluatedTemplateComponent
		test_component_round_trip(
			R"({
			"type": "evaluated_template",
			"info_id": "template_msg_id"
		})",
			"evaluated_template"
		);

		// Test MessageIDComponent
		test_component_round_trip(
			R"({
			"type": "message_id",
			"info_id": "some_message_id"
		})",
			"message_id"
		);

		// ===== Structure Tests =====

		// Test PointerMessage
		test_from_json_to_json_equality(
			R"({
			"pointer_message_id": "error_location",
			"message_id": "msg_123"
		})",
			dia_file::PointerMessage::fromJson
		);

		// Test Metadata
		test_from_json_to_json_equality(
			R"({
            "template_type": "message",
			"type": "error",
			"family": "type_error",
			"name": "undefined_type"
		})",
			dia_file::Metadata::fromJson
		);

		// Test ExploreEdge
		test_from_json_to_json_equality(
			R"({
			"name": "see_definition",
			"params": {
				"location": {"type": "text", "content": "line 42"}
			}
		})",
			dia_file::ExploreEdge::fromJson
		);

		// Test Message
		test_from_json_to_json_equality(
			R"({
			"metadata": {
                "template_type": "message",
				"type": "error",
				"family": "type_error",
				"name": "type_mismatch"
			},
			"params": {
				"expected": {"type": "code", "content": "i32"},
				"found": {"type": "code", "content": "f64"}
			},
			"explore_edges": [
            {
                "name": "find_definition_edge",
                "params": {
                   "type": {"type": "code", "content": "some_variable_name"},
                   "link_target": {"type": "message_id", "info_id": "msg_var_def"}
                }
            }]
		})",
			dia_file::Message::fromJson
		);

		// Test Entity
		test_from_json_to_json_equality(
			R"({
			"assoc_infos": ["msg_1", "msg_2", "msg_3"]
		})",
			dia_file::Entity::fromJson
		);

		// Test Thread
		test_from_json_to_json_equality(
			R"({
			"main_message": {
				"metadata": {
                    "template_type": "message",
					"type": "error",
					"family": "compile_error",
					"name": "syntax_error"
				},
				"params": {},
				"explore_edges": [
                {
                    "name": "find_definition_edge",
                    "params": {
                        "type": {"type": "code", "content": "some_variable_name"},
                        "link_target": {"type": "message_id", "info_id": "msg_var_def"}
                    }
                }]
			},
			"displayed_attached_messages": ["attach_1"],
			"attached_messages": {
				"attach_1": {
					"metadata": {
						"type": "note",
						"family": "hint",
						"name": "suggestion"
					},
					"params": {}
				}
			},
			"entities": {
				"entity_1": {
					"assoc_infos": ["msg_x"]
				}
			}
		})",
			dia_file::Thread::fromJson
		);
	}

	// ========================================
	// YAML Template Deserialization Tests
	// ========================================

	void yamlTemplateDeserialization() {
		// Helper lambda to test YAML deserialization and verify component type
		auto test_yaml_component = [this]<typename T>(const std::string& yaml_str) {
			YAML::Node yaml      = YAML::Load(yaml_str);
			auto       component = template_file::Component::fromYaml(yaml);

			// Verify correct type was deserialized
			auto* typed_comp = dynamic_cast<T*>(component.get());
			ASSERT_EQUAL(false, typed_comp == nullptr);
		};

		// Test TextComponent
		test_yaml_component.template operator(
		)<template_file::TextComponent>("\"Hello from template\"");

		// Test ParamComponent
		test_yaml_component.template operator(
		)<template_file::ParamComponent>("param: \"type_name\"");

		// Test ConcatComponent
		test_yaml_component.template operator()<template_file::ConcatComponent>(R"(
- "Error: type "
- param: "type"
- " not found"
)");

		// Test MacroComponent
		test_yaml_component.template operator(
		)<template_file::MacroComponent>("macro: \"format_type\"");

		// Test CaseOfComponent
		test_yaml_component.template operator()<template_file::CaseOfComponent>(R"(
case:
  param: "is_signed"
of:
  "true": "signed"
  "[other]": "unsigned"
)");

		// Test CodeBlockComponent
		test_yaml_component.template operator()<template_file::CodeBlockComponent>(R"(
codeblock:
  - "int x = 42;"
  - "return x;"
)");

		// Test MessageLinkComponent
		test_yaml_component.template operator()<template_file::MessageLinkComponent>(R"(
content: "See documentation"
url:
  param: "doc_message_id"
)");

		// Test IsParamProvidedComponent
		test_yaml_component.template operator(
		)<template_file::IsParamProvidedComponent>("is_param_provided: \"optional_param\"");

		// Test complete DiagnosticTemplate (MessageTemplate variant)
		{
			std::string complete_template_yaml = R"(
metadata:
  template_type: message
  type: error
  family: type_error
  name: type_mismatch
  code: 1001
  active_from: "1.0.0"
  active_until: "2.0.0"

params:
  expected:
    description: "Expected type"
    component_type: "code"
    optional: false
  found:
    description: "Found type"
    component_type: "code"
    optional: false

macros:
  format_type:
    - "Type: "
    - param: "expected"

header_message:
  - "Type mismatch: expected "
  - param: "expected"
  - ", but found "
  - param: "found"

description:
  - "The types do not match."

explore_edges:
  see_definition:
    name: "See definition"
    params:
      location:
        description: "Definition location"
        optional: false

pointer_messages:
  error_here:
    type: "error"
    priority: 1
    content: "Error occurs here"
)";

			YAML::Node yaml                = YAML::Load(complete_template_yaml);
			auto       diagnostic_template = template_file::DiagnosticTemplate::fromYaml(yaml);

			// Verify it's a MessageTemplate variant
			auto* message_template
				= std::get_if<template_file::MessageTemplate>(&diagnostic_template.content);
			ASSERT_EQUAL(false, message_template == nullptr);

			// Verify metadata
			ASSERT_EQUAL("error", message_template->metadata.type);
			ASSERT_EQUAL("type_error", message_template->metadata.family);
			ASSERT_EQUAL("type_mismatch", message_template->metadata.name);
			ASSERT_EQUAL(1'001, message_template->metadata.code);

			// Verify params
			ASSERT_EQUAL(true, message_template->params.contains("expected"));
			ASSERT_EQUAL(true, message_template->params.contains("found"));

			// Verify macros
			ASSERT_EQUAL(true, message_template->macros.contains("format_type"));

			// Verify explore edges
			ASSERT_EQUAL(true, message_template->explore_edges.contains("see_definition"));

			// Verify pointer messages
			ASSERT_EQUAL(true, message_template->pointer_messages.contains("error_here"));
		}

		// Test ComponentTemplate variant
		{
			std::string component_template_yaml = R"(
metadata:
  template_type: component
  type: info
  family: helper
  name: code_snippet
  code: 2001
  active_from: "1.0.0"
  active_until: ""

params:
  code:
    description: "Code to display"
    component_type: "code"
    optional: false

macros: {}

content:
  codeblock:
    - param: "code"
)";

			YAML::Node yaml                = YAML::Load(component_template_yaml);
			auto       diagnostic_template = template_file::DiagnosticTemplate::fromYaml(yaml);

			// Verify it's a ComponentTemplate variant
			auto* component_template
				= std::get_if<template_file::ComponentTemplate>(&diagnostic_template.content);
			ASSERT_EQUAL(false, component_template == nullptr);

			// Verify metadata
			ASSERT_EQUAL("info", component_template->metadata.type);
			ASSERT_EQUAL("helper", component_template->metadata.family);
			ASSERT_EQUAL("code_snippet", component_template->metadata.name);
		}

		// Test PointerMessageTemplate variant
		{
			std::string pointer_template_yaml = R"(
metadata:
  template_type: pointer_message
  type: note
  family: hint
  name: suggestion
  code: 3001
  active_from: "1.0.0"
  active_until: ""

params:
  suggestion_text:
    description: "Suggestion text"
    optional: false

macros: {}

pointer_messages:
  hint_location:
    type: "hint"
    priority: 5
    content:
      param: "suggestion_text"
)";

			YAML::Node yaml                = YAML::Load(pointer_template_yaml);
			auto       diagnostic_template = template_file::DiagnosticTemplate::fromYaml(yaml);

			// Verify it's a PointerMessageTemplate variant
			auto* pointer_template
				= std::get_if<template_file::PointerMessageTemplate>(&diagnostic_template.content);
			ASSERT_EQUAL(false, pointer_template == nullptr);

			// Verify metadata
			ASSERT_EQUAL("note", pointer_template->metadata.type);
			ASSERT_EQUAL("hint", pointer_template->metadata.family);
			ASSERT_EQUAL("suggestion", pointer_template->metadata.name);

			// Verify pointer messages
			ASSERT_EQUAL(true, pointer_template->pointer_messages.contains("hint_location"));
		}
	}

	// ========================================
	// Template Evaluation Tests
	// ========================================

	void evaluateSimpleTextTemplate() {
		// Create a simple text template
		TemplateRegistry::setInstance(makeBox<TemplateRegistryTestProvider>(
			base::HashMap<std::string, std::string>{ { "type/family/name", "<yaml content>" } }
		));
		auto diagnostic_file = dia_file::Thread::fromJson(json::parse(R"({
                "metadata": {
                    "template_type": "message",
                    "type": "error",
                    "family": "type_error",
                    "name": "simple_error"
                },
                "params": {}
            })"));

		state::Diagnostic diagnostic = evaluateDiagnostic(diagnostic_file);


        auto result = constructTextView(diagnostic.messages[0].header.ref());
        

		// Note: We can't actually test full evaluation without a TemplateRegistry
		// but we can verify the template structure is correct
		ASSERT_EQUAL("Error occurred", template_comp->text);
	}

	void evaluateParamTemplate() {
		// Create a param template
		auto template_comp = base::makeBox<template_file::ParamComponent>("error_message");

		// Verify structure
		ASSERT_EQUAL("error_message", template_comp->param);
	}

	void evaluateConcatTemplate() {
		// Create a concat template
		std::vector<Box<template_file::Component>> elements;
		elements.push_back(base::makeBox<template_file::TextComponent>("Type "));
		elements.push_back(base::makeBox<template_file::ParamComponent>("type_name"));
		elements.push_back(base::makeBox<template_file::TextComponent>(" not found"));

		auto template_comp = base::makeBox<template_file::ConcatComponent>(std::move(elements));

		// Verify structure
		ASSERT_EQUAL(3, template_comp->elements.size());
	}

	void evaluateCaseOfTemplate() {
		// Create a case-of template
		auto pattern = base::makeBox<template_file::ParamComponent>("is_signed");
		base::Map<std::string, Box<template_file::Component>> cases;
		cases.put("true", base::makeBox<template_file::TextComponent>("signed"));
		cases.put("false", base::makeBox<template_file::TextComponent>("unsigned"));

		auto template_comp
			= base::makeBox<template_file::CaseOfComponent>(std::move(pattern), std::move(cases));

		// Verify structure - access the pattern through the Box
		ASSERT_EQUAL(
			"is_signed", dynamic_cast<template_file::ParamComponent&>(*template_comp->pattern).param
		);

		ASSERT_EQUAL(2, template_comp->cases.size());
	}
};

TESTER_COMMON_MAIN("/src/common/diagnostic_interactive/core/tests/");
