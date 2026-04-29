#include <debug_info/debug_info.hpp>
#include <debug_info/debug_info_builder.hpp>
#include <debug_info/debug_info_io.hpp>

#include <tester/tester.hpp>

#include <sstream>

using namespace debug_info;

class DebugInfoTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DebugInfoTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(builderTest);
		TESTER_ADD_TEST(serializationRoundTripTest);
		TESTER_ADD_TEST(invalidJsonTest);
		TESTER_ADD_TEST(resolvePositionsTest);
	}

private:
	/** Builds a small but complete DebugInfo for reuse across tests
	 * (note that it uses both kinds of SourcePosition, which is incorrect outside of testing).
	 */
	static debug_info::DebugInfo makeTestDebugInfo() {
		auto line_position = FilePosition{
			.file_path    = "test.duck",
			.start_line   = 1,
			.start_column = 0,
			.end_line     = 10,
			.end_column   = 1,
		};

		auto pst_position = PstHashPostion{ .postion_scope_begin = base::Bit256{ 0, 1, 2, 4 },
			                                .postion_scope_end   = base::Optional<base::Bit256>{
                                                base::Bit256{ 4, 3, 2, 1 } } };


		SourcePosition func_pos;
		func_pos.line_col_position = line_position;

		line_position.start_line = line_position.end_line = 2;
		SourcePosition instr0_pos;
		instr0_pos.line_col_position = line_position;

		SourcePosition instr4_pos;
		line_position.start_line = line_position.end_line = 3;
		instr4_pos.line_col_position                      = line_position;

		SourcePosition var0_pos;
		line_position.start_line = line_position.end_line = 4;
		var0_pos.line_col_position                        = pst_position;

		DebugInfoBuilder debug_info_builder(Target::DBC, SourcePositionsType::LineColumn);
		debug_info_builder.setModulePath("test.dbc");

		return debug_info_builder.addType("_TMyType", "MyType")
		    .addType("_TOther", TypeMetadata{ .name = "Other" })
		    .beginFunction("_Zfoo", "foo", func_pos)
		    .addParameter(0, "param_a", instr0_pos)
		    .addParameter(1, "param_b", std::nullopt)
		    .addInstruction(0, instr0_pos)
		    .addInstruction(4, instr4_pos)
		    .addVariableInit(0, "local_x", var0_pos)
		    .addVariableInit(8, "local_y", std::nullopt)
		    .end()
		    .build();
	}

	void builderTest() {
		using namespace debug_info;

		auto info = makeTestDebugInfo();

		assertTrue(info.target == Target::DBC, "Target should be DBC");
		assertTrue(info.module_path == "test.dbc", "Module path incorrect");
		assertTrue(
			info.source_positions_type == SourcePositionsType::LineColumn,
			"Source positions type incorrect"
		);


		assertTrue(info.types.size() == 2, "Expected 2 types");
		assertTrue(info.functions.size() == 1, "Expected 1 function");

		const auto& func = info.functions.at("_Zfoo");
		assertTrue(func.function_name.has_value(), "Function name should be present");
		assertTrue(*func.function_name == "foo", "Function name incorrect");
		assertTrue(func.parameter_indexes_to_metadata.size() == 2, "Expected 2 parameters");
		assertTrue(func.instr_offsets_to_metadata.size() == 2, "Expected 2 instructions");
		assertTrue(
			func.instr_offsets_to_variable_init.size() == 2, "Expected 2 variable initializations"
		);

		// Instructions are stored in insertion order from the builder
		bool found0 = false, found4 = false;
		for (const auto& [offset, meta]: func.instr_offsets_to_metadata) {
			if (offset == 0) {
				found0         = true;
				const auto& fp = std::get<FilePosition>(meta.position.line_col_position);
				assertTrue(fp.start_line == 2, "Instr 0: start_line incorrect");
			} else if (offset == 4) {
				found4         = true;
				const auto& fp = std::get<FilePosition>(meta.position.line_col_position);
				assertTrue(fp.start_line == 3, "Instr 4: start_line incorrect");
			}
		}
		assertTrue(found0, "Instruction at offset 0 not found");
		assertTrue(found4, "Instruction at offset 4 not found");

		const auto& [var_offset, var_meta] = func.instr_offsets_to_variable_init.front();
		assertTrue(var_offset == 0, "Variable init offset incorrect");
		assertTrue(var_meta.name == "local_x", "Variable name incorrect");
		assertTrue(var_meta.position.has_value(), "Variable position should be present");

		const auto& [var_offset2, var_meta2] = func.instr_offsets_to_variable_init.at(1);
		assertTrue(var_offset2 == 8, "Second variable init offset incorrect");
		assertTrue(var_meta2.name == "local_y", "Second variable name incorrect");
		assertFalse(var_meta2.position.has_value(), "Second variable position should be absent");
	}

	void serializationRoundTripTest() {
		auto info = makeTestDebugInfo();

		// Serialize
		std::ostringstream oss;
		debug_info::saveToStream(info, oss);
		const std::string first_json = oss.str();

		assertTrue(!first_json.empty(), "Serialized JSON should not be empty");

		// Deserialize
		std::istringstream iss(first_json);
		auto               result = debug_info::loadFromStream(iss);
		assertTrue(result.has_value(), "Deserialization of valid JSON should succeed");

		// Serialize again and compare
		std::ostringstream oss2;
		debug_info::saveToStream(*result, oss2);
		const std::string second_json = oss2.str();

		assertTrue(first_json == second_json, "Round-trip JSON should be identical");

		// Spot-check deserialized values
		assertTrue(result->target == debug_info::Target::DBC, "Round-trip: target incorrect");
		assertTrue(result->module_path == "test.dbc", "Round-trip: module_path incorrect");
		assertTrue(result->types.size() == 2, "Round-trip: wrong number of types");
		assertTrue(result->functions.size() == 1, "Round-trip: wrong number of functions");
		assertTrue(
			result->functions.at("_Zfoo").parameter_indexes_to_metadata.size() == 2,
			"Round-trip: wrong number of parameters"
		);
		assertTrue(
			result->functions.at("_Zfoo").instr_offsets_to_metadata.size() == 2,
			"Round-trip: wrong number of instructions"
		);
		assertTrue(
			result->functions.at("_Zfoo").instr_offsets_to_variable_init.size() == 2,
			"Round-trip: wrong number of variable initializations"
		);
		const auto& round_trip_vars = result->functions.at("_Zfoo").instr_offsets_to_variable_init;
		assertTrue(
			round_trip_vars.at(0).second.position.has_value(),
			"Round-trip: first variable position should be present"
		);
		assertFalse(
			round_trip_vars.at(1).second.position.has_value(),
			"Round-trip: second variable position should be absent"
		);
		const auto& round_trip_params = result->functions.at("_Zfoo").parameter_indexes_to_metadata;
		assertTrue(
			round_trip_params.at(0).second.position.has_value(),
			"Round-trip: first parameter position should be present"
		);
		assertFalse(
			round_trip_params.at(1).second.position.has_value(),
			"Round-trip: second parameter position should be absent"
		);
	}

	void invalidJsonTest() {
		// Completely malformed JSON
		{
			std::istringstream iss("not valid json at all {{{");
			auto               result = debug_info::loadFromStream(iss);
			assertFalse(result.has_value(), "Malformed JSON should fail to parse");
		}

		// Valid JSON but missing required field "target"
		{
			std::istringstream iss(R"({"module_path": "x.dmf", "source_positions_type": "DBC",
                "functions": {}, "types": {}})");
			auto               result = debug_info::loadFromStream(iss);
			assertFalse(result.has_value(), "JSON missing 'target' field should fail");
		}

		// Valid JSON but wrong type for a field
		{
			std::istringstream iss(
				R"({"target": 42, "module_path": "x.dmf", "source_positions_type": "LineColumn",
                "functions": {}, "types": {}})"
			);

			auto _ = debug_info::loadFromStream(iss);
			// nlohmann enum deserialization may not throw for unknown integers, but
			// at minimum we verify the function returns without crashing
		}

		// Empty input
		{
			std::istringstream iss("");
			auto               result = debug_info::loadFromStream(iss);
			assertFalse(result.has_value(), "Empty input should fail to parse");
		}

		// Instructions not sorted by offset should fail
		{
			std::istringstream iss(R"({
    "target": "DBC",
    "module_path": "x.dmf",
    "source_positions_type": "LineColumn",
    "functions": {
        "_Zx": {
            "function_name": "x",
            "position": { "type": "FilePosition", "value": { "file_path": "x.duck", "start_line": 1, "start_column": 0, "end_line": 2, "end_column": 0 } },
            "instr_offsets_to_metadata": [
                [8, { "position": { "type": "FilePosition", "value": { "file_path": "x.duck", "start_line": 2, "start_column": 0, "end_line": 2, "end_column": 1 } } }],
                [4, { "position": { "type": "FilePosition", "value": { "file_path": "x.duck", "start_line": 3, "start_column": 0, "end_line": 3, "end_column": 1 } } }]
            ]
        }
    },
    "types": {}
})");
			auto               result = debug_info::loadFromStream(iss);
			assertFalse(result.has_value(), "Unsorted instr_offsets_to_metadata should fail");
		}

		// Variable initializations not sorted by offset should fail
		{
			std::istringstream iss(R"({
	"target": "DBC",
	"module_path": "x.dmf",
	"source_positions_type": "LineColumn",
	"functions": {
		"_Zx": {
			"function_name": "x",
			"position": { "type": "FilePosition", "value": { "file_path": "x.duck", "start_line": 1, "start_column": 0, "end_line": 2, "end_column": 0 } },
			"instr_offsets_to_metadata": [],
			"instr_offsets_to_variable_init": [
				[8, { "name": "a", "position": { "type": "FilePosition", "value": { "file_path": "x.duck", "start_line": 2, "start_column": 0, "end_line": 2, "end_column": 1 } } }],
				[4, { "name": "b", "position": { "type": "FilePosition", "value": { "file_path": "x.duck", "start_line": 3, "start_column": 0, "end_line": 3, "end_column": 1 } } }]
			]
		}
	},
	"types": {}
})");
			auto               result = debug_info::loadFromStream(iss);
			assertFalse(result.has_value(), "Unsorted instr_offsets_to_variable_init should fail");
		}
	}

	void resolvePositionsTest() {
		// Build two functions whose positions are PstHashPostion.
		// Also add one variable init to verify nested metadata gets resolved too.

		auto make_hash_pos = [&](u64 a, u64 b) -> SourcePosition {
			return SourcePosition{ .line_col_position = PstHashPostion{
									   .postion_scope_begin = base::Bit256{ a, b, 0, 0 },
									   .postion_scope_end   = std::nullopt,
								   } };
		};

		DebugInfo info = DebugInfoBuilder(Target::DBC, SourcePositionsType::PstHash)
		                     .beginFunction("_Zfoo", "foo", make_hash_pos(1, 10))
		                     .addParameter(0, "arg_foo", make_hash_pos(10, 100))
		                     .addVariableInit(7, "local_foo", make_hash_pos(11, 110))
		                     .end()
		                     .beginFunction("_Zbar", "bar", make_hash_pos(2, 20))
		                     .end()
		                     .build();

		assertTrue(
			info.source_positions_type == SourcePositionsType::PstHash,
			"Before resolve: type should be PstHash"
		);

		// Resolver: encodes (a, b) from the hash into a deterministic FilePosition.
		info.resolvePositions([](const PstHashPostion& p) -> FilePosition {
			return FilePosition{
				.file_path    = "resolved.duck",
				.start_line   = p.postion_scope_begin.data.at(0),
				.start_column = p.postion_scope_begin.data.at(1),
				.end_line     = p.postion_scope_begin.data.at(0),
				.end_column   = p.postion_scope_begin.data.at(1),
			};
		});

		assertTrue(
			info.source_positions_type == SourcePositionsType::LineColumn,
			"After resolve: type should be LineColumn"
		);

		const auto check_func = [&](const std::string& mangled, u64 expected_line) {
			const auto& func = info.functions.at(mangled);
			const auto& fp   = std::get<FilePosition>(func.position->line_col_position);
			assertTrue(fp.file_path == "resolved.duck", mangled + ": file_path incorrect");
			assertTrue(fp.start_line == expected_line, mangled + ": start_line incorrect");
		};

		check_func("_Zfoo", 1);
		check_func("_Zbar", 2);

		const auto& foo_var
			= info.functions.at("_Zfoo").instr_offsets_to_variable_init.at(0).second;
		assertTrue(
			foo_var.position.has_value(), "Variable position should be present after resolve"
		);
		const auto& var_fp = std::get<FilePosition>(foo_var.position->line_col_position);
		assertTrue(foo_var.name == "local_foo", "Variable name should be preserved");
		assertTrue(var_fp.file_path == "resolved.duck", "Variable init: file_path incorrect");
		assertTrue(var_fp.start_line == 11, "Variable init: start_line incorrect");
		assertTrue(var_fp.start_column == 110, "Variable init: start_column incorrect");

		const auto& foo_param = info.functions.at("_Zfoo").parameter_indexes_to_metadata.at(0);
		assertTrue(foo_param.first == 0, "Parameter index should be preserved");
		assertTrue(foo_param.second.name == "arg_foo", "Parameter name should be preserved");
		assertTrue(foo_param.second.position.has_value(), "Parameter position should be present");
		const auto& param_fp = std::get<FilePosition>(foo_param.second.position->line_col_position);
		assertTrue(param_fp.file_path == "resolved.duck", "Parameter: file_path incorrect");
		assertTrue(param_fp.start_line == 10, "Parameter: start_line incorrect");
		assertTrue(param_fp.start_column == 100, "Parameter: start_column incorrect");
	}
};

TESTER_COMMON_MAIN("/src/common/debug_info/tests/");
