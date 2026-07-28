#include <helios/mangler/demangler.hpp>

#include <base/str/str_utils.hpp>

#include <tester/tester.hpp>

#include <string>
#include <string_view>

using namespace compiler::helios::mangler;

/**
 * Tests of the de-mangler, i.e. the inverse of the mangling scheme described in
 * `src/helios/mangler/mangling-scheme.md`.
 *
 * The mangled names used here are the ones the mangler really produces - the ones with metadata are
 * copied straight from the `ASSERT_EQUAL`s of `helios_test.cpp`'s mangler tests, so that both sides
 * stay pinned to the same scheme.
 */
class DemanglerTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DemanglerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(notMangledNames);
		TESTER_ADD_TEST(globalsAndConstants);
		TESTER_ADD_TEST(functions);
		TESTER_ADD_TEST(builtinTypes);
		TESTER_ADD_TEST(compositeTypes);
		TESTER_ADD_TEST(typeModifiers);
		TESTER_ADD_TEST(specialSymbols);
		TESTER_ADD_TEST(modifiedFunctionTypes);
		TESTER_ADD_TEST(operators);
		TESTER_ADD_TEST(replWrappers);
		TESTER_ADD_TEST(brokenNames);
	}

private:
	/**
	 * @brief Asserts that `mangled_name` de-mangles into `expected`.
	 */
	void assertDemangles(const std::string_view mangled_name, const std::string_view expected) {
		const auto demangled = tryDemangle(mangled_name);
		assertTrue(demangled.has_value(), base::strConcat("Failed to demangle ", mangled_name, "."));
		ASSERT_EQUAL_PRINT(std::string(expected), demangled.value());
	}

	/**
	 * @brief Asserts that `mangled_name` cannot be de-mangled, and that demangle() passes it
	 * through unchanged.
	 */
	void assertNotDemanglable(const std::string_view mangled_name) {
		assertTrue(
			!tryDemangle(mangled_name).has_value(),
			base::strConcat("Unexpectedly demangled ", mangled_name, ".")
		);
		ASSERT_EQUAL(std::string(mangled_name), demangle(mangled_name));
	}

	/**
	 * @brief Builds a mangled name of scheme version 0 out of a bare `<encoding>`.
	 */
	static std::string versionZero(const std::string_view encoding) {
		return base::strConcat("_Q_", encoding);
	}

	/**
	 * @brief De-mangles `fun m.f(a: <type>) -> void` to get at the spelling of `type`.
	 */
	void assertParameterType(const std::string_view type, const std::string_view expected) {
		assertDemangles(
			versionZero(base::strConcat("M1mG1fFv", type, "E1aE")),
			base::strConcat("m.f(a: ", expected, ") -> void")
		);
	}

	void notMangledNames() {
		// Symbols with C linkage keep their source name, there is nothing to de-mangle.
		assertNotDemanglable("main");
		assertNotDemanglable("builtin_output_i64");
		assertNotDemanglable("");
	}

	void globalsAndConstants() {
		assertDemangles("_Q_M8manglingG1A", "mangling.A");
		assertDemangles(
			"_Q5a_M8manglingN5Nmspc1BE$metadata_v321", "mangling.Nmspc.B $metadata_v321"
		);
		assertDemangles(
			"_Q5a_M8manglingN4Mspc3Ooo4CnstE$metadata_v321", "mangling.Mspc.Ooo.Cnst $metadata_v321"
		);
		// A submodule contributes one more component to the path prefix.
		assertDemangles(
			"_Q4_M8mangling3subN5inSub8subConstE$metadata_v5",
			"mangling.sub.inSub.subConst $metadata_v5"
		);
		// Classes are symbols too, and they say so.
		assertDemangles("_Q_CM8manglingN4Mspc3Ooo3ClsE", "class mangling.Mspc.Ooo.Cls");
	}

	void functions() {
		assertDemangles(
			"_Q1Y_M8manglingN4Mspc3Ooo5gooooEFi32i32f64E1a1bE$metadata_v123",
			"mangling.Mspc.Ooo.goooo(a: i32, b: f64) -> i32 $metadata_v123"
		);
		assertDemangles(
			"_Q4_M8mangling3subN5inSub6subFunEFi32EE$metadata_v5",
			"mangling.sub.inSub.subFun() -> i32 $metadata_v5"
		);
		// A parameter-less signature without metadata, straight off a global function.
		assertDemangles(versionZero("M1mG3fooFvEE"), "m.foo() -> void");
	}

	void builtinTypes() {
		assertParameterType("u", "()");
		assertParameterType("v", "void");
		assertParameterType("y", "byte");
		assertParameterType("b", "bool");
		assertParameterType("c", "char");
		assertParameterType("t", "type");
		assertParameterType("i32", "i32");
		assertParameterType("j8", "u8");
		assertParameterType("f64", "f64");
		assertParameterType("p", "raw_pointer");
	}

	void compositeTypes() {
		assertParameterType("Pi32E", "ptr i32");
		assertParameterType("MPi32E", "manyptr i32");
		assertParameterType("CPi32E", "cptr i32");
		assertParameterType("Si32E", "slice i32");
		assertParameterType("Di32E", "List[i32]");
		assertParameterType("A4i32E", "i32[4]");
		assertParameterType("Ti32f64E", "Tuple(i32, f64)");
		assertParameterType("Vi32bE", "Variant(i32, bool)");
		assertParameterType("Fvi32E", "Function(i32) -> (void)");
		// Nesting: a pointer to a list of tuples.
		assertParameterType("PDTi32cEEE", "ptr List[Tuple(i32, char)]");
		// A class used as a type embeds the whole mangled name of the class symbol.
		assertParameterType("_Q_CM1mG3Cls", "m.Cls");
	}

	void typeModifiers() {
		assertParameterType("Ni32", "const i32");
		assertParameterType("Ri32", "ref i32");
		assertParameterType("Xi32", "box i32");
		assertParameterType("Mi32", "unique i32");
		assertParameterType("Li32", "leaking i32");
		assertParameterType("MLNRi32", "unique leaking const ref i32");
	}

	void specialSymbols() {
		assertDemangles("_Q_M8manglingGHmcE", "mangling.<module constructor>");
		assertDemangles("_Q_M8manglingGHmdE", "mangling.<module destructor>");
		assertDemangles(
			"_Q4_M20mangling_special_memG1Agc", "mangling_special_mem.A.<global constructor>"
		);
		assertDemangles(
			"_Q4_M20mangling_special_memG1Agd", "mangling_special_mem.A.<global destructor>"
		);
		// Generated constructors are owned by the type they construct.
		assertDemangles(
			versionZero("i32HicFi32i32E1vEE"), "i32.<implicit constructor>(v: i32) -> i32"
		);
		assertDemangles(
			versionZero("_Q_CM1mG3ClsHdcFvEEE"), "m.Cls.<default constructor>() -> void"
		);
		// Generated methods have no path at all - the signature disambiguates them.
		assertDemangles(versionZero("HddFvPi32EE1aEE"), "<default destructor>(a: ptr i32) -> void");
		assertDemangles(versionZero("HtoStringFvEEE"), "<toString>() -> void");
		assertDemangles(versionZero("HbaFPi32EEEE"), "<box alloc>() -> ptr i32");
	}

	/**
	 * Names taken verbatim out of a `core` bytecode artifact. The function type of a generated
	 * method is mangled as a `const` symbol type, so the signature starts with a `<type-modifier>`
	 * rather than with `F`.
	 */
	void modifiedFunctionTypes() {
		assertDemangles(
			"_Q_HddNFNuR_Q_CM4core10containersG6StringE2_0EE",
			"<default destructor>(_0: ref core.containers.String) -> const ()"
		);
		assertDemangles("_Q_HlengthNFj64NScEE2_0EE", "<length>(_0: const slice char) -> u64");
		assertDemangles(
			"_Q_HtoStringNF_Q_CM4core10containersG6StringNf64E2_0EE",
			"<toString>(_0: const f64) -> core.containers.String"
		);
	}

	void operators() {
		// "O" <operatoriness> <length> <op-translit-unit>*, see mangling-scheme.md.
		assertDemangles(
			versionZero("M1mN3ClsOi4plplEFi32i32E1aE"), "m.Cls.operator++(a: i32) -> i32"
		);
		assertDemangles(
			versionZero("M1mN3ClsOp2plEFi32i32E1aE"), "m.Cls.prefix operator+(a: i32) -> i32"
		);
		assertDemangles(
			versionZero("M1mN3ClsOs4mimiEFi32i32E1aE"), "m.Cls.suffix operator--(a: i32) -> i32"
		);
		// Anything that is not one of the fixed tags is escaped as "x" <hex-codepoint> "_".
		assertDemangles(
			versionZero("M1mN3ClsOi4x40_EFi32i32E1aE"), "m.Cls.operator@(a: i32) -> i32"
		);
	}

	void replWrappers() {
		assertDemangles(versionZero("__repl_expr_wrapper_7"), "<repl expression 7>");
		assertDemangles(versionZero("__repl_instr_wrapper_0"), "<repl instruction 0>");
	}

	void brokenNames() {
		assertNotDemanglable("_Q");                    // no scheme version
		assertNotDemanglable("_Q_");                   // no encoding
		assertNotDemanglable("_Q_M8mangling");         // no symbol name
		assertNotDemanglable("_Q_M8manglingN1AE1B");   // trailing garbage
		assertNotDemanglable("_Q_M8manglingG1");       // identifier shorter than promised
		assertNotDemanglable("_Q_M8manglingG1AFqEE");  // unknown type in the signature
		assertNotDemanglable("_Q_M8manglingGHzzE");    // unknown special symbol
		assertNotDemanglable("_Q_B0_");                // back-references are not emitted yet
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
