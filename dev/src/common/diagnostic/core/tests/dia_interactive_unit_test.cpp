#include <yaml-cpp/yaml.h>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/core/diagnostic_component_traversal.hpp>
#include <diagnostic/core/template_evaluation.hpp>
#include <diagnostic/core/template_file.hpp>
#include <tester/tester.hpp>

#include <json/json.hpp>

using namespace dia;

class DiagnosticTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DiagnosticTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// JSON serialization/deserialization tests
		TESTER_ADD_TEST(jsonComponentsRoundTrip);
		TESTER_ADD_TEST(complexComponentJsonRoundTrip);

		// JSON string round trip tests
		TESTER_ADD_TEST(jsonStringRoundTrip);
		TESTER_ADD_TEST(codeLocationHashDeserialization);
		TESTER_ADD_TEST(componentTraversalAndCodeLocationUpdate);

		// YAML template deserialization tests
		TESTER_ADD_TEST(yamlTemplateDeserialization);
	}

private:
	// ========================================
	// JSON Serialization/Deserialization Tests
	// ========================================

	void jsonComponentsRoundTrip() {
		// Helper lambda to test component round-trip
		auto test_round_trip
			= [this](Box<dia_args::Component> component, const std::string& expected_type) {
				  // Serialize to JSON
				  json j = component->toJson();

				  // Verify type field
				  ASSERT_EQUAL(expected_type, j["type"].get<std::string>());

				  // Deserialize back
				  auto deserialized = dia_args::Component::fromJson(j);

				  // Verify deserialized type matches
				  ASSERT_EQUAL(expected_type, deserialized->toJson()["type"].get<std::string>());
			  };

		// Test TextComponent
		test_round_trip(base::makeBox<dia_args::TextComponent>("Hello, World!"), "text");

		// Test CodeComponent
		test_round_trip(base::makeBox<dia_args::CodeComponent>("int x = 42;"), "code");

		// Test ConcatComponent
		{
			std::vector<Box<dia_args::Component>> elements;
			elements.emplace_back(base::makeBox<dia_args::TextComponent>("Error: "));
			elements.emplace_back(base::makeBox<dia_args::CodeComponent>("variable"));
			test_round_trip(base::makeBox<dia_args::ConcatComponent>(std::move(elements)), "concat");
		}

		// Test LinkComponent
		{
			auto content = base::makeBox<dia_args::CodeComponent>("MyType");
			test_round_trip(
				base::makeBox<dia_args::LinkComponent>(
					std::vector<std::string>{ "abc" }, std::move(content)
				),
				"link"
			);
		}

		// Test PointedComponent
		{
			auto content = base::makeBox<dia_args::CodeComponent>("x + y");
			std::vector<dia_args::PointerMessage> pointer_messages;
			pointer_messages.emplace_back(
				"error_location", base::Optional<dia_args::MessageID>("msg_1")
			);
			pointer_messages.emplace_back("hint_location", base::Optional<dia_args::MessageID>());
			test_round_trip(
				base::makeBox<dia_args::PointedComponent>(
					std::move(content), std::move(pointer_messages)
				),
				"pointed"
			);
		}
	}

	void complexComponentJsonRoundTrip() {
		// Create a complex nested structure
		std::vector<Box<dia_args::Component>> inner_elements;
		inner_elements.emplace_back(base::makeBox<dia_args::TextComponent>("Type "));

		auto link_content = base::makeBox<dia_args::CodeComponent>("i32");
		inner_elements.emplace_back(base::makeBox<dia_args::LinkComponent>(
			std::vector<std::string>{ "abc" }, std::move(link_content)
		));

		auto inner_concat = base::makeBox<dia_args::ConcatComponent>(std::move(inner_elements));

		std::vector<Box<dia_args::Component>> outer_elements;
		outer_elements.emplace_back(std::move(inner_concat));
		outer_elements.emplace_back(
			base::makeBox<dia_args::TextComponent>(" is a signed 32-bit integer")
		);

		auto original = base::makeBox<dia_args::ConcatComponent>(std::move(outer_elements));

		// Serialize to JSON
		json j = original->toJson();

		// Deserialize back
		auto deserialized = dia_args::Component::fromJson(j);

		// Verify structure
		auto* outer_concat = dynamic_cast<dia_args::ConcatComponent*>(deserialized.get());
		ASSERT_EQUAL(false, outer_concat == nullptr);
		ASSERT_EQUAL(2, outer_concat->elements.size());

		auto* inner_concat_deser
			= dynamic_cast<dia_args::ConcatComponent*>(outer_concat->elements[0].get());
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
				  auto component = dia_args::Component::fromJson(j);

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
				  std::cout << j.dump(2) << '\n';
				  std::cout << j2.dump(2) << '\n';
				  ASSERT_EQUAL(j.dump(2), j2.dump(2));
			  };

		// ===== Component Tests =====

		// Test TextComponent
		test_component_round_trip(R"({"type": "text", "content": "Hello from JSON"})", "text");

		// Test CodeComponent
		test_component_round_trip(R"({"type": "code", "content": "int x = 42;"})", "code");

		// Test CodeLocationComponent
		test_component_round_trip(
			R"({
			"type": "code_location",
			"file": "main.cpp",
			"line": 10,
			"column": 5
		})",
			"code_location"
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

		// Test LinkComponent
		test_component_round_trip(
			R"({
			"type": "link",
			"content": {"type": "code", "content": "MyType"},
			"target_messages": ["msg_abc", "msg_def"]
		})",
			"link"
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
			dia_args::PointerMessage::fromJson
		);

		// Test Metadata
		test_from_json_to_json_equality(
			R"({
            "template_type": "message",
			"type": "error",
			"family": "type_error",
			"name": "undefined_type"
		})",
			dia_args::Metadata::fromJson
		);

		// Test ExploreEdge
		test_from_json_to_json_equality(
			R"({
			"name": "see_definition",
			"params": {
				"location": {"type": "text", "content": "line 42"}
			}
		})",
			dia_args::ExploreLink::fromJson
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
			"explore_links": [
            {
                "name": "find_definition_edge",
                "params": {
                   "type": {"type": "code", "content": "some_variable_name"},
                   "link_target": {"type": "message_id", "info_id": "msg_var_def"}
                }
            }]
		})",
			dia_args::Message::fromJson
		);

		// Test Diagnostic
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
				"explore_links": [
                {
                    "name": "find_definition_edge",
                    "params": {
                        "type": {"type": "code", "content": "some_variable_name"},
                        "link_target": {"type": "message_id", "info_id": "msg_var_def"}
                    }
                }],
				"attached_messages": ["attach_1"]
			},
			"linked_messages": {
				"attach_1": {
					"metadata": {
                        "template_type": "message",
						"type": "note",
						"family": "hint",
						"name": "suggestion"
					},
					"params": {}
				}
			}
		})",
			dia_args::Diagnostic::fromJson
		);
	}

	void codeLocationHashDeserialization() {
		const std::string code_location_with_hash_json = R"({
			"type": "code_location",
			"file": "main.cpp",
			"line": 10,
			"column": 5,
			"hash_location": {
				"begin_node": [1, 2, 3, 4],
				"end_node": [5, 6, 7, 8]
			}
		})";

		json  j             = json::parse(code_location_with_hash_json);
		auto  component     = dia_args::Component::fromJson(j);
		auto* code_location = dynamic_cast<dia_args::CodeLocationComponent*>(component.get());

		ASSERT_TRUE(code_location != nullptr);
		ASSERT_HAS_VALUE(code_location->hash_location);
		ASSERT_EQUAL(1, code_location->hash_location->begin_node.data[0]);
		ASSERT_EQUAL(2, code_location->hash_location->begin_node.data[1]);
		ASSERT_EQUAL(3, code_location->hash_location->begin_node.data[2]);
		ASSERT_EQUAL(4, code_location->hash_location->begin_node.data[3]);
		ASSERT_HAS_VALUE(code_location->hash_location->end_node);
		ASSERT_EQUAL(5, code_location->hash_location->end_node->data[0]);
		ASSERT_EQUAL(6, code_location->hash_location->end_node->data[1]);
		ASSERT_EQUAL(7, code_location->hash_location->end_node->data[2]);
		ASSERT_EQUAL(8, code_location->hash_location->end_node->data[3]);

		json j2 = component->toJson();
		ASSERT_EQUAL(j.dump(2), j2.dump(2));
	}

	void componentTraversalAndCodeLocationUpdate() {
		auto diagnostic = dia_args::Diagnostic::fromJson(json::parse(R"json({
			"main_message": {
				"metadata": {
					"template_type": "message",
					"type": "error",
					"family": "test",
					"name": "main"
				},
				"params": {
					"location_direct": {
						"type": "code_location",
						"file": "a.dk",
						"line": 1,
						"column": 1,
						"hash_location": {
							"begin_node": [11, 0, 0, 0]
						}
					},
					"nested": {
						"type": "concat",
						"content": [
							{ "type": "code", "content": "x" },
							{
								"type": "link",
								"target_messages": ["note_1"],
								"content": {
									"type": "code_location",
									"file": "a.dk",
									"line": 2,
									"column": 2
								}
							}
						]
					}
				},
				"explore_links": []
			},
			"linked_messages": {
				"note_1": {
					"metadata": {
						"template_type": "message",
						"type": "note",
						"family": "test",
						"name": "linked"
					},
					"params": {
						"location_linked": {
							"type": "code_location",
							"file": "b.dk",
							"line": 3,
							"column": 3,
							"hash_location": {
								"begin_node": [22, 0, 0, 0]
							}
						},
						"some_code": { "type": "code", "content": "y" }
					}
				}
			}
		})json"));

		usize code_location_count = 0;
		usize code_count          = 0;

		dia_args::forEachComponentInDiagnostic<dia_args::CodeLocationComponent>(
			diagnostic,
			[&code_location_count](dia_args::CodeLocationComponent&) { ++code_location_count; }
		);
		dia_args::forEachComponentInDiagnostic<dia_args::CodeComponent>(
			diagnostic, [&code_count](dia_args::CodeComponent&) { ++code_count; }
		);

		ASSERT_EQUAL(3, code_location_count);
		ASSERT_EQUAL(2, code_count);
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
location:
  param: "location"
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

explore_links:
  see_definition:
    content: "See definition"
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
			ASSERT_EQUAL(true, message_template->explore_links.contains("see_definition"));

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
};

TESTER_COMMON_MAIN("/src/common/diagnostic/core/tests/");
