#include <algorithm>
#include <tester/tester.hpp>
#include <typesystem/typesystem.hpp>

class NewTypeSystemTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS NewTypeSystemTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Simple TypeSystem Test new interface") {
		TESTER_ADD_TEST(simple_void);
		TESTER_ADD_TEST(simple_byte_sized);
		TESTER_ADD_TEST(simple_ints);
		TESTER_ADD_TEST(simple_floats);
		TESTER_ADD_TEST(simple_pointer);
		TESTER_ADD_TEST(simple_enum_and_flag);
		TESTER_ADD_TEST(simple_option);
		TESTER_ADD_TEST(simple_tuple);
		TESTER_ADD_TEST(simple_variant);
		TESTER_ADD_TEST(simple_type_template);
		TESTER_ADD_TEST(simple_namespace_code_block_and_module);
		TESTER_ADD_TEST(simple_meta);
		TESTER_ADD_TEST(simple_type_desc);
		TESTER_ADD_TEST(simple_function);
	}

private:
	using enum ts::Kind;

	void simple_void() {
		auto void_1 = ts::VoidInfo::create();
		auto void_2 = ts::VoidInfo::create();
		assert(void_1 == void_2, "There shouldn't be multiple different 'void' types.");
		assert(void_1.getKind() == Void, "Void is not a void");

		ts::TypeInfo type_1 = void_1;
		assert(type_1.getKind() == Void, "Void as type is not a void");

		ts::VoidInfo void_3 = type_1;
		assert(void_3.getKind() == Void, "Void as type as void is not a void");
	}

	void simple_byte_sized() {
		auto byte_i = ts::ByteInfo::create();
		auto bool_i = ts::BoolInfo::create();
		auto char_i = ts::CharInfo::create();

		assert(byte_i != bool_i && bool_i != char_i && char_i != byte_i,
		       "All byte-sized types should be different.");

		auto byte_ii = ts::ByteInfo::create();
		auto bool_ii = ts::BoolInfo::create();
		auto char_ii = ts::CharInfo::create();

		assert(byte_i == byte_ii && bool_i == bool_ii && char_i == char_ii,
		       "Byte-sized types of the same kind should be equal.");

		assert(byte_i.getKind() == Byte, "Byte is not a byte.");
		ts::TypeInfo type_byte = byte_i;
		assert(type_byte.getKind() == Byte, "Byte as type is not a byte.");
		ts::ByteInfo byte_iii = type_byte;
		assert(byte_iii.getKind() == Byte, "Byte as type as byte is not a byte.");

		assert(bool_i.getKind() == Bool, "Bool is not a bool.");
		ts::TypeInfo type_bool = bool_i;
		assert(type_bool.getKind() == Bool, "Bool as type is not a bool.");
		ts::BoolInfo bool_iii = type_bool;
		assert(bool_iii.getKind() == Bool, "Bool as type as bool is not a bool.");

		assert(char_i.getKind() == Char, "Char is not a char.");
		ts::TypeInfo type_char = char_i;
		assert(type_char.getKind() == Char, "Char as type is not a char.");
		ts::CharInfo char_iii = type_char;
		assert(char_iii.getKind() == Char, "Char as type as char is not a char.");
	}

	void simple_ints() {
		for (usize i = 0; i < 5; i++) {
			auto int_i  = ts::IntegralInfo::create(8 * (1 << i));
			auto int_ii = ts::IntegralInfo::create(8 * (1 << i));
			auto int_u  = ts::IntegralInfo::create(8 * (1 << i), false);

			assert(int_i.getSize() == 8 * (1 << i), "Size of simple int is incorrect.");
			assert(int_i == int_ii, "Ints of the same size and signedness should be the same.");
			assert(int_i != int_u,
			       "Ints of the same size but different signedness should be the different.");
			assert(int_i.getKind() == Integral, "Ints should have integral as kind.");

			ts::TypeInfo type = int_i;
			assert(type.getKind() == Integral, "Int as type is not int.");
			ts::IntegralInfo int_iii = type;
			assert(int_iii.getKind() == Integral, "Int as type as int is not int.");
		}

		assert(ts::IntegralInfo::create(8) != ts::IntegralInfo::create(16),
		       "Ints of different sizes should be different.");
	}

	void simple_floats() {
		usize float_sizes[] = { 16, 32, 64, 80, 128 };
		for (usize float_size : float_sizes) {
			auto float_i  = ts::FloatInfo::create(float_size);
			auto float_ii = ts::FloatInfo::create(float_size);

			assert(float_i.getSize() == float_size, "Size of simple float is incorrect.");
			assert(float_i == float_ii, "Floats of the same size should be the same.");
			assert(float_i.getKind() == Float, "Floats should have integral kind.");

			ts::TypeInfo type = float_i;
			assert(type.getKind() == Float, "Float as type is not float.");
			ts::FloatInfo float_iii = type;
			assert(float_iii.getKind() == Float, "Float as type as float is not float.");
		}

		assert(ts::FloatInfo::create(32) != ts::IntegralInfo::create(64),
		       "Floats of different sizes should be different.");
	}

	void simple_pointer() {
		auto raw  = ts::RawPointerInfo::create();
		auto raw2 = ts::RawPointerInfo::create();
		assert(raw == raw2, "There shouldn't be multiple different 'raw pointer' types.");

		ts::TypeInfo       type = raw;
		ts::RawPointerInfo raw3 = type;
		assert(raw.getKind() == RawPointer, "Raw Pointer is not a raw pointer");
		assert(type.getKind() == RawPointer, "Raw Pointer as Type is not a raw pointer");
		assert(raw3.getKind() == RawPointer,
		       "Raw Pointer as Type as raw pointer is not a raw pointer");

		ts::TypeDesc<>     desc(raw);
		auto               pointer     = ts::PointerInfo::create(desc);
		ts::RawPointerInfo raw_pointer = pointer;
		assert(pointer.getKind() == Pointer, "Pointer is not a pointer");
		assert(raw_pointer.getKind() == Pointer, "Pointer as Raw Pointer is not a pointer");

		ts::PointerInfo pointer2 = raw_pointer;
		assert(pointer2.getKind() == Pointer, "Pointer as Raw Pointer as pointer is not a pointer");
		assert(pointer2.getUnderlying() == desc, "Pointer forgot its underlying type description");
	}

	void simple_function() {
		auto int_16 = ts::TypeDesc<>(ts::IntegralInfo::create(16));
		auto int_32 = ts::TypeDesc<>(ts::IntegralInfo::create(32));

		auto fun    = ts::FunctionInfo::create({ int_16, int_32 }, int_32);

		assert(fun.getParameterTypeList() == std::vector<ts::TypeDesc<>>({ int_16, int_32 }),
		       "Wrong parameter types.");
		assert(fun.getResultType() == int_32, "Wrong result type.");

		assert(fun.getKind() == Function, "Function is not a function");
		ts::TypeInfo type = fun;
		assert(type.getKind() == Function, "Function as type is not a function");
		ts::FunctionInfo fun2 = type;
		assert(fun2.getKind() == Function, "Function as type as function is not a function");

		auto fun_identical = ts::FunctionInfo::create({ int_16, int_32 }, int_32);
		assert(fun == fun_identical,
		       "Functions of the same input and output types should be the same.");
		auto fun_different_flags = ts::FunctionInfo::create({ int_16, int_32 }, int_32, 1);
		assert(fun != fun_different_flags, "Functions with different flags should be different.");
		auto fun_different_input = ts::FunctionInfo::create({ int_32, int_32 }, int_32);
		assert(fun != fun_different_input,
		       "Functions of different input types should be different.");
		auto fun_different_output = ts::FunctionInfo::create({ int_16, int_32 }, int_16);
		assert(fun != fun_different_output,
		       "Functions of different output types should be different.");
	}

	void simple_enum_and_flag() {
		auto int_8  = ts::IntegralInfo::create(8);

		auto enum_1 = ts::EnumInfo::create(int_8);
		assert(enum_1.getKind() == Enum, "Enum should have kind enum.");
		assert(enum_1.getBaseType() == int_8, "Enum base type should be enum base type.");
		assert(enum_1.getSize() == int_8.getSize(),
		       "Enum should have same size as underlying type.");

		ts::TypeInfo type_1 = enum_1;
		ts::EnumInfo enum_2 = type_1;
		assert(type_1.getKind() == Enum, "Enum as type should have kind enum.");
		assert(enum_2.getKind() == Enum, "Enum as type as enum should have kind enum.");

		auto flag_1 = ts::FlagInfo::create(int_8);
		assert(flag_1.getKind() == Flag, "Flag should have kind flag.");
		assert(flag_1.getBaseType() == int_8, "Flag base type should be flag base type.");
		assert(flag_1.getSize() == int_8.getSize(),
		       "Flag should have same size as underlying type.");

		ts::TypeInfo type_2 = flag_1;
		ts::FlagInfo flag_2 = type_2;
		assert(type_2.getKind() == Flag, "Flag as type should have kind flag.");
		assert(flag_2.getKind() == Flag, "Flag as type as flag should have kind flag.");
	}

	void simple_option() {
		auto int_8    = ts::IntegralInfo::create(8);
		auto int_desc = ts::TypeDesc<ts::IntegralInfo>(int_8);
		auto opt_1    = ts::OptionalInfo::create(int_desc);

		assert(opt_1.getKind() == Optional, "Option type should have kind optional.");
		assert(opt_1.getSize() == int_8.getSize() + ts::BYTE_SIZE,
		       "Option type should have size of underlying type.");
		assert(opt_1.getUnderlying() == int_desc,
		       "Option underlying desc should be type desc from construction.");

		auto type_1 = opt_1;
		auto opt_2  = type_1;

		assert(type_1.getKind() == Optional, "Option as Type should have kind optional.");
		assert(opt_2.getKind() == Optional, "Option as Type as Option should have kind optional.");
	}

	void simple_tuple() {
		auto           void_i  = ts::VoidInfo::create();
		auto           my_enum = ts::EnumInfo::create(ts::IntegralInfo::create(8));

		ts::TypeDesc<> desc_1(void_i);
		ts::TypeDesc<> desc_2(my_enum);

		auto           tup_1  = ts::TupleInfo::create({ desc_1, desc_2 });
		auto           tup_1_ = ts::TupleInfo::create({ desc_1, desc_2 });
		auto           tup_2  = ts::TupleInfo::create({ desc_2, desc_1 });

		assert(tup_1.getKind() == Tuple, "Tuple is not a tuple");

		assert(tup_1 != tup_2, "Tuples with different types should be the different.");

		assert(tup_1 == tup_1_, "Tuples with same types should be the same.");

		assert(tup_1.getSize() == desc_1.getType().getSize() + desc_2.getType().getSize(),
		       "Wrong tuple size.");
	}

	void simple_variant() {
		auto           int_8   = ts::IntegralInfo::create(8);
		auto           my_enum = ts::EnumInfo::create(int_8);

		ts::TypeDesc<> desc_1(int_8);
		ts::TypeDesc<> desc_2(my_enum);

		auto           var_1  = ts::VariantInfo::create({ desc_1, desc_2 });
		auto           var_1_ = ts::VariantInfo::create({ desc_1, desc_2 });
		auto           var_2  = ts::VariantInfo::create({ desc_2, desc_1 });

		assert(var_1.getKind() == Variant, "Variant is not a variant");
		assert(var_1 != var_2, "Variants with different types should be the different.");
		assert(var_1 == var_1_, "Variants with same types should be the same.");

		assert(var_1.getSize()
		           == std::max(desc_1.getType().getSize(), desc_2.getType().getSize())
		                  + ts::BYTE_SIZE,
		       "Wrong variant size.");
	}

	void simple_type_template() {
		auto                        int64          = ts::IntegralInfo::create(64);
		auto                        int64_desc     = ts::TypeDesc<ts::IntegralInfo>(int64);
		auto                        flag64         = ts::FlagInfo::create(int64);
		auto                        flag64_desc    = ts::TypeDesc<ts::FlagInfo>(flag64);

		std::vector<ts::TypeDesc<>> int_parameter  = { int64_desc };
		std::vector<ts::TypeDesc<>> flag_parameter = { flag64_desc };
		auto                        typeTemplate_1 = ts::TypeTemplateInfo::create(int_parameter);
		auto                        typeTemplate_2 = ts::TypeTemplateInfo::create(int_parameter);
		auto                        typeTemplate_3 = ts::TypeTemplateInfo::create(flag_parameter);

		assert(typeTemplate_1.getKind() == TypeTemplate, "Kind is not TypeTemplate.");
		assert(typeTemplate_1.getParameterList() == int_parameter,
		       "TypeTemplate parameter list is incorrect.");
		assert(typeTemplate_1 == typeTemplate_2,
		       "TypeTemplates with same parameters should be the same.");
		assert(typeTemplate_1 != typeTemplate_3,
		       "TypeTemplates with different parameters should be different.");
	}

	void simple_namespace_code_block_and_module() {
		auto ns = ts::NamespaceInfo::create();
		auto cb = ts::CodeBlockInfo::create();
		auto md = ts::ModuleInfo::create();

		assert(ns.getKind() == Namespace, "Kind is not 'Namespace'.");
		assert(cb.getKind() == CodeBlock, "Kind is not 'CodeBlock'.");
		assert(md.getKind() == Module, "Kind is not 'Module'.");
		assert(ns == ts::NamespaceInfo::create(), "Namespaces should be equal.");
		assert(cb == ts::CodeBlockInfo::create(), "CodeBlocks should be equal.");
		assert(md == ts::ModuleInfo::create(), "Modules should be equal.");
	}

	void simple_meta() {
		auto meta   = ts::MetaInfo::create();
		auto meta_2 = ts::MetaInfo::create();

		assert(meta == meta_2, "There shouldn't be multiple different 'type' types");
		assert(meta.getSize() == ts::META_SIZE, "MetaType should have size META_SIZE");

		assert(meta.getKind() == Meta, "MetaType should have kind Meta");
	}

	void simple_type_desc() {
		auto                           void_i = ts::VoidInfo::create();
		auto                           int_i  = ts::IntegralInfo::create(8);

		ts::TypeDesc<ts::IntegralInfo> int_desc(int_i);

		try {
			ts::TypeDesc<ts::IntegralInfo>{ void_i };
			fail("Created IntegralDesc for Void type.");
		} catch (const base::LogicError&) {
			// expected
		}

		ts::TypeDesc<>{ void_i };

		ts::TypeDesc<> really_int_desc = int_desc;

		assert(int_desc.getType().getKind() == Integral, "IntDesc type is not int");
		assert(really_int_desc.getType().getKind() == Integral,
		       "IntDesc type after conversion to TypeDesc is not int");
	}

public:
	~NewTypeSystemTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/src/typesystem/tests/")
