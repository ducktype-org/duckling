#include <yaml-cpp/yaml.h>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/core/diagnostic_state.hpp>
#include <diagnostic/core/template_evaluation.hpp>
#include <diagnostic/core/template_file.hpp>
#include <diagnostic/core/template_registry.hpp>
#include <diagnostic/core/view_constructors.hpp>
#include <tester/tester.hpp>

#include <json/json.hpp>

using namespace dia;

class DiagnosticE2ETester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DiagnosticE2ETester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(evaluateSimpleTextTemplate);
		TESTER_ADD_TEST(evaluateComplexComponentTemplate);
		TESTER_ADD_TEST(evaluatePointerMessageTemplate);
	}

private:
	void evaluateSimpleTextTemplate() {
		// Create a simple text template
		TemplateRegistrySingleton::setNewInstance(base::makeBox<TemplateRegistryTestProvider>(
			base::HashMap<std::string, std::string>{ { "error/type_error/simple_error", R"(
metadata:
  template_type: message
  type: error
  family: type_error
  name: simple_error
  code: 1
  active_from: "1.0.0"
  active_until: ""

params: {}
macros: {}

header_message:
  - "Error occurred"

description: []
explore_links: {}
pointer_messages: {}
)" } }
		));
		auto diagnostic_file = dia_args::Diagnostic::fromJson(json::parse(R"({
            "main_message": {
                "metadata": {
                    "template_type": "message",
                    "type": "error",
                    "family": "type_error",
                    "name": "simple_error"
                },
                "params": {}
            },
            "linked_messages": {},
            "entities": {}
            })"));

		state::Diagnostic diagnostic = evaluateDiagnostic(diagnostic_file);


		auto result = constructTextView(diagnostic.messages[0].header.ref());


		// Note: We can't actually test full evaluation without a TemplateRegistry
		// but we can verify the template structure is correct
		ASSERT_EQUAL("Error occurred", result);
	}

	void evaluateComplexComponentTemplate() {
		// Register templates
		TemplateRegistrySingleton::setNewInstance(base::makeBox<TemplateRegistryTestProvider>(
			base::HashMap<std::string, std::string>{ { "error/type_check/no_match_2op_new", R"(
metadata:
  template_type: message
  type: error
  family: type_check
  name: no_match_2op_new
  code: 1001
  active_from: "0.0.1"
  active_until: "now"

params:
  operator:
    description: "The operator which does not match."
  left_type:
    description: "Type of the left operand"
    component_type: "evaluated_template"
  right_type:
    description: "Type of the right operand"
    component_type: "evaluated_template"
  code:
    description: "The code snippet"
    component_type: "concat"
  location:
    description: "Location"
    component_type: "code_location"

macros: {}

header_message:
  - "no matching `"
  - param: "operator"
  - "` operator for types "
  - param: "left_type"
  - " and "
  - param: "right_type"

description:
  - codeblock:
      param: "code"
    location:
      param: "location"

explore_links: {}
pointer_messages:
  cause:
    priority: 1
    type: "error"
    content:
        - "used here"
)" },
		                                             { "component/general/type_with_alias", R"yaml(
metadata:
  template_type: component
  type: component
  family: general
  name: type_with_alias
  code: 0
  active_from: "0.0.1"
  active_until: "now"

params:
  type_name:
    description: "The type name"
  is_static:
    description: "Is static"
  expanded_type:
    description: "Expanded type"
    optional: true

macros: {}

content:
  - case:
      param: "is_static"
    of:
      "true":
        - "`"
        - param: "type_name"
        - "`"
        - case:
            is_param_provided: "expanded_type"
          of:
            true:
              - " (aka `"
              - param: "expanded_type"
              - "`)"
            false: []
      "false":
        - "`"
        - case:
            is_param_provided: "expanded_type"
          of:
            true:
              - default:
                - param: "type_name"
                alternative:
                - param: "expanded_type"
            false:
              - param: "type_name"
        - "`"
)yaml" } }
		));

		// Construct diagnostic file from JSON
		auto diagnostic_file = dia_args::Diagnostic::fromJson(json::parse(R"({
    "main_message": {
        "metadata": {
            "template_type": "message",
            "type": "error",
            "family": "type_check",
            "name": "no_match_2op_new"
        },
        "params": {
            "operator": {
                "type": "code",
                "content": "+"
            },
            "left_type": {
                "type": "evaluated_template",
                "info_id": "0"
            },
            "right_type": {
                "type": "evaluated_template",
                "info_id": "1"
            },
            "code": {
                "type": "concat",
                "content": [
                    { "type": "start_line", "number": 30 },
                    { "type": "code", "content": "var x: i64 = " },
                    { "type": "code", "content": "1 ? 2" },
                    {
                        "type": "pointed",
                        "content": { "type": "code", "content": "; " },
                        "pointer_messages": [
                            { "pointer_message_id": "cause" }
                        ]
                    }
                ]
            },
            "location": {
                "type": "code_location",
                "file": "example.dk",
                "line": 1,
      				"column": 1,
      				"hash_location": {
      					"begin_node": [10, 11, 12, 13],
      					"end_node": [14, 15, 16, 17]
      				}
            }
        }
    },
    "linked_messages": {
        "0": {
            "metadata": {
                "template_type": "component",
                "type": "component",
                "family": "general",
                "name": "type_with_alias"
            },
            "params": {
                "type_name": { "type": "code", "content": "i32" },
                "is_static": { "type": "text", "content": "true" }
            }
        },
        "1": {
            "metadata": {
                "template_type": "component",
                "type": "component",
                "family": "general",
                "name": "type_with_alias"
            },
            "params": {
                "type_name": { "type": "code", "content": "std::string" },
                "is_static": { "type": "text", "content": "true" },
                "expanded_type": { "type": "code", "content": "std::vector<u8>" }
            }
        }
    },
    "entities": {}
})"));

		// Evaluate
		state::Diagnostic diagnostic = evaluateDiagnostic(diagnostic_file);

		// Verify header text
		auto result = constructTextView(diagnostic.messages[0].header.ref());
		ASSERT_EQUAL(
			"no matching `+` operator for types `i32` and `std::string` (aka `std::vector<u8>`)",
			result
		);

		// Verify description (code block)
		ASSERT_HAS_VALUE(diagnostic.messages[0].description.ref().toOpt());
		auto desc_result
			= constructTextView(diagnostic.messages[0].description.ref().toOpt().value());
		// constructTextView for CodeBlockComponent visits its content
		// The content is: start_line(30) + "var x: i64 = " + "1 ? 2" + "; "
		// StartLineComponent adds "\n"
		ASSERT_EQUAL("\nvar x: i64 = 1 ? 2; ", desc_result);

		// Verify location in CodeBlockComponent
		auto* concat = dynamic_cast<const state::ConcatComponent*>(
			diagnostic.messages[0].description.ref().toOpt().value().get()
		);
		ASSERT_EQUAL(false, concat == nullptr);
		ASSERT_EQUAL(1, concat->components.size());

		auto* code_block
			= dynamic_cast<const state::CodeBlockComponent*>(concat->components[0].get());
		ASSERT_EQUAL(false, code_block == nullptr);
		ASSERT_HAS_VALUE(code_block->location);
		ASSERT_EQUAL("example.dk", code_block->location.value().file);
		ASSERT_EQUAL(1, code_block->location.value().line);
		ASSERT_EQUAL(1, code_block->location.value().column);
	}

	void evaluatePointerMessageTemplate() {
		// Register templates
		TemplateRegistrySingleton::setNewInstance(base::makeBox<TemplateRegistryTestProvider>(
			base::HashMap<std::string, std::string>{ { "error/overload/call_failed", R"yaml(
metadata:
  template_type: message
  type: error
  family: overload
  name: call_failed
  code: 2001
  active_from: "0.0.1"
  active_until: "now"

params:
  code:
    description: "The code snippet"
    component_type: "concat"
  location:
    description: "Location"
    component_type: "code_location"

macros: {}

header_message:
  - "No matching function for call"

description:
  - codeblock:
      param: "code"
    location:
      param: "location"

explore_links:
  see_candidate:
    content:
      url:
        param: "link_target"
      content:
        - "See candidate "
        - param: "function_signature"
    params:
      link_target:
        description: "Target message"
        component_type: "message_id"
      function_signature:
        description: "Function signature"
        component_type: "code"

pointer_messages: {}
)yaml" },
		                                             { "note/overload/candidate", R"yaml(
metadata:
  template_type: message
  type: note
  family: overload
  name: candidate
  code: 2002
  active_from: "0.0.1"
  active_until: "now"

params:
  code:
    description: "The function definition"
    component_type: "concat"
  location:
    description: "Location"
    component_type: "code_location"

macros: {}

header_message:
  - "Candidate function:"

description:
  - codeblock:
      param: "code"
    location:
      param: "location"

explore_links: {}

pointer_messages:
  mismatch:
    priority: 1
    type: "error"
    content: []
)yaml" },
		                                             { "note/overload/no_conversion", R"yaml(
metadata:
  template_type: pointer_message
  type: note
  family: overload
  name: no_conversion
  code: 3001
  active_from: "0.0.1"
  active_until: "now"

params:
  from_type:
    description: "From type"
    component_type: "code"
  to_type:
    description: "To type"
    component_type: "code"

macros: {}

pointer_messages:
  failed_match:
    type: "error"
    priority: 1
    content:
      - "no conversion found from "
      - param: "from_type"
      - " to "
      - param: "to_type"
)yaml" } }
		));

		// Construct diagnostic file from JSON
		auto diagnostic_file = dia_args::Diagnostic::fromJson(json::parse(R"json({
    "main_message": {
        "metadata": {
            "template_type": "message",
            "type": "error",
            "family": "overload",
            "name": "call_failed"
        },
        "params": {
            "code": {
                "type": "concat",
                "content": [
                    { "type": "start_line", "number": 50 },
                    { "type": "code", "content": "my_func(1, 2, \"hello\");" }
                ]
            },
            "location": {
                "type": "code_location",
                "file": "main.cpp",
                "line": 50,
                "column": 1
            }
        },
        "explore_links": [
            {
                "name": "see_candidate",
                "params": {
                    "link_target": { "type": "message_id", "info_id": "msg_candidate" },
                    "function_signature": { "type": "code", "content": "my_func(i32, i32, i32)" }
                }
            }
        ],
        "attached_messages": ["msg_candidate"]
    },
    "linked_messages": {
        "msg_candidate": {
            "metadata": {
                "template_type": "message",
                "type": "note",
                "family": "overload",
                "name": "candidate"
            },
            "params": {
                "code": {
                    "type": "concat",
                    "content": [
                        { "type": "start_line", "number": 10 },
                        { "type": "code", "content": "fun my_func(a: i32, b: i32, " },
                        {
                            "type": "pointed",
                            "content": { "type": "code", "content": "c: i32" },
                            "pointer_messages": [
                                {
                                    "pointer_message_id": "failed_match",
                                    "message_id": "msg_reason"
                                }
                            ]
                        },
                        { "type": "code", "content": ")" }
                    ]
                },
                "location": {
                    "type": "code_location",
                    "file": "main.cpp",
                    "line": 10,
                    "column": 1
                }
            }
        },
        "msg_reason": {
            "metadata": {
                "template_type": "pointer_message",
                "type": "note",
                "family": "overload",
                "name": "no_conversion"
            },
            "params": {
                "from_type": {
                    "type": "code",
                    "content": "string"
                },
                "to_type": {
                    "type": "code",
                    "content": "i32"
                }
            }
        }
    },
    "entities": {}
})json"));

		// Evaluate
		state::Diagnostic diagnostic = evaluateDiagnostic(diagnostic_file);

		// Verify we have the main message and the candidate message
		// The main message is index 0.
		// The candidate message should be index 1 because it's attached to main message.
		ASSERT_EQUAL(2, diagnostic.messages.size());

		// Verify Main Message Header
		auto main_header = constructTextView(diagnostic.messages[0].header.ref());
		ASSERT_EQUAL("No matching function for call", main_header);

		// Verify Candidate Message Header
		auto candidate_header = constructTextView(diagnostic.messages[1].header.ref());
		ASSERT_EQUAL("Candidate function:", candidate_header);

		// Verify Candidate Description (code block)
		ASSERT_HAS_VALUE(diagnostic.messages[1].description.ref().toOpt());

		// Verify pointer message content in Candidate Message
		auto* concat = dynamic_cast<const state::ConcatComponent*>(
			diagnostic.messages[1].description.ref().toOpt().value().get()
		);
		ASSERT_EQUAL(false, concat == nullptr);
		// concat -> codeblock -> concat -> [start_line, code, code(pointed), code]

		auto* code_block
			= dynamic_cast<const state::CodeBlockComponent*>(concat->components[0].get());
		ASSERT_EQUAL(false, code_block == nullptr);

		auto* inner_concat = dynamic_cast<const state::ConcatComponent*>(code_block->content.get());
		ASSERT_EQUAL(false, inner_concat == nullptr);
		ASSERT_EQUAL(4, inner_concat->components.size());

		// Component 0: StartLine
		// Component 1: "fun my_func(a: i32, b: i32, "
		// Component 2: "c: i32" (This should have the pointer message)
		// Component 3: ")"

		auto* code_comp_pointed
			= dynamic_cast<const state::CodeComponent*>(inner_concat->components[2].get());
		ASSERT_EQUAL(false, code_comp_pointed == nullptr);
		ASSERT_EQUAL("c: i32", code_comp_pointed->content);
		ASSERT_EQUAL(1, code_comp_pointed->pointer_messages.size());

		// Verify the pointer message content
		auto pm_id = code_comp_pointed->pointer_messages[0];
		auto it    = diagnostic.messages[1].pointer_messages.find(pm_id);
		ASSERT_EQUAL(true, it != diagnostic.messages[1].pointer_messages.end());

		const auto& pm = it->second;
		ASSERT_EQUAL("no conversion found from string to i32", pm.content);

		// Verify explore edges
		const auto& explore_links = diagnostic.messages[0].explore_links;
		ASSERT_EQUAL(1, explore_links.size());
		ASSERT_EQUAL("see_candidate", explore_links[0].name);
		auto& edge = explore_links[0];
		// Verify content of the edge
		auto result_edge = constructTextView(edge.content.ref());
		ASSERT_EQUAL("See candidate my_func(i32, i32, i32)", result_edge);
	}
};

TESTER_COMMON_MAIN("/src/common/diagnostic/core/tests/");
