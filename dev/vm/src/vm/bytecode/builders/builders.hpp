#pragma once

#include "../instructions.hpp"

#include <base/maps.hpp>
#include <base/ref.hpp>
#include <base/string_id.hpp>
#include <base/stringifyable_enum.hpp>
#include <base/strongly_typed_id.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/bytecode/opcode_args.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/utils/stable_type_id_name_map.hpp>

#include <cstdint>
#include <utility>
#include <vector>


/**
 * @brief Builder-level instruction kind to set which instruction to build.
 */
// Disable liting, because of invalid naming convention.
// NOLINTBEGIN
// clang-format off
MAKE_STRINGIFYABLE_ENUM(vm::code::builders, std::uint8_t, OpKind,
	init,
	deinit,
	mov,
	cmov,
	add,
	sub,
	mul,
	div,
	mod,
	neg,
	cmpEq,
	cmpG,
	jmp,
	jmpIf,
	jmpIfNot,
	call,
	ret,
	ret_tailcall,
	input,
	output,
	alloc,
	free,
	load,
	store,
	setVTable,

	/**
	 *  Do not use directly. If an instruction supports `ext` opcodes,
	 *  just push another argument to the instruction builder.
	 */
	ext,
	exit
)
// clang-format on
// NOLINTEND

namespace vm::code::builders {
	class TypeContextBuilder;

	/**
	 * @brief TypeContext contains built types.
	 * It can be used built using TypesContext<TypesContextState::AddingTypes>.
	 */
	class TypeContext {
		friend TypeContextBuilder;
		TypeContext() = default;

		Box<TypeMetadata>               metadata = makeBox<TypeMetadata>();
		StableTypeIdNameMap<TypeOfData> types;

	public:
		[[nodiscard]] const StableTypeIdNameMap<TypeOfData>& getTypes() const;

		[[nodiscard]] const TypeMetadata& getMetadata() const;

		Box<TypeMetadata> moveMetadata() &&;
	};

	/**
	 * @brief TypeContextBuilder allows for adding types.
	 * It is used to build TypeContext.
	 */
	class TypeContextBuilder {
		using FieldVector = std::vector<std::pair<base::StrID, TypeRef>>;
		StableTypeIdNameMap<TypeOfData> types;

		/**
		 * @brief Retrieves the TypeOfData variant for a given type name, throws UnknownSubtypeError
		 * if the type is not found.
		 */
		const vm::code::TypeOfData& getTypeOfData(base::StrID name, const TypeOfData& context) const;

		/**
		 * @brief Retrieves a specific type from a TypeOfData variant, throws UnknownSubtypeError if
		 * the type is not found and an error returned by the error factory if the expected type
		 * mismatches the actual one.
		 */
		template<typename ExpectedType, typename ErrorFactory>
		const ExpectedType& getType(
			base::StrID name, const vm::code::TypeOfData& context, ErrorFactory error_factory
		) const;

		/**
		 * @brief Recursively collects fields from a FieldableType (ClassType or DataType) and its
		 * superclasses.
		 */
		template<typename FieldableType>
		void collectFieldsRecursive(
			const FieldableType&                     fieldable,
			const TypeOfData&                        error_context,
			base::HashMap<base::StrID, base::StrID>& fields
		) const;

		/**
		 * @brief Recursively collects virtual methods from an InheritableType (ClassType or
		 * InterfaceType), its implemented interfaces, and superclasses (for ClassType).
		 */
		template<typename InheritableType, typename ErrorContextType>
		void collectVirtualMethodsRecursive(
			const InheritableType&                   inh,
			const ErrorContextType&                  error_context_inh,
			base::HashMap<base::StrID, base::StrID>& virtual_methods
		) const;

		/**
		 * @brief Recursively collects method implementations from an InheritableType (ClassType or
		 * InterfaceType), its implemented interfaces, and superclasses.
		 */
		template<typename InheritableType, typename ErrorContextType>
		void collectImplementationsRecursive(
			const InheritableType&                   inh,
			const ErrorContextType&                  error_context_inh,
			base::HashMap<base::StrID, base::StrID>& virtual_methods
		) const;

		/**
		 * @brief Validates that a method's first argument is a pointer to the 'this' object.
		 */
		template<typename InheritableType>
		void validateMethodFirstArgument(const InheritableType& inh, const FunctionType& func_type)
			const;

		/**
		 * @brief Validates that a method implementation's signature matches a virtual method's
		 * signature.
		 */
		template<typename InheritableType>
		void validateMethodSignatureMatch(
			const InheritableType& inh,
			const FunctionType&    vmethod_type,
			const FunctionType&    impl_type
		) const;

		/**
		 * @brief Validates the signatures of all virtual methods declared in an InheritableType.
		 */
		template<typename InheritableType>
		void validateVMethodSignatures(const InheritableType& inh) const;

		/**
		 * @brief Validates all method implementations within an InheritableType.
		 */
		template<typename InheritableType>
		void validateImplementations(
			const InheritableType&                         inh,
			const base::HashMap<base::StrID, base::StrID>& virtual_methods
		) const;

		/**
		 * @brief Validates that all virtual methods of a non-abstract class are implemented.
		 */
		void validateAllMethodsImplemented(
			const ClassType& clazz, const base::HashMap<base::StrID, base::StrID>& virtual_methods
		) const;

		/**
		 * @brief Validates that there are no duplicate field names within a FieldableType and its
		 * hierarchy.
		 */
		template<typename FieldableType>
		void validateFieldDuplicates(const FieldableType& fieldable) const;

		/**
		 * @brief Validates that an InheritableType does not list the same interface multiple times
		 * in its direct `implements` list.
		 */
		template<typename InheritableType>
		void validateImplementsDuplicates(const InheritableType& implements) const;

		/**
		 * @brief Recursively builds a vtable layout for an InheritableType.
		 */
		template<typename InheritableType, typename ErrorContextType>
		void buildVTableRecursive(
			const InheritableType&                inh,
			const ErrorContextType&               error_context_inh,
			base::HashMap<base::StrID, TypeCRef>& implementations,
			const TypeContext&                    tctx
		) const;

		/**
		 * @brief Builds a vector of fields (FieldVector) for an InheritableType. Collects all
		 * fields from superclasses.
		 */
		template<typename FieldableType>
		FieldVector buildFieldVector(const FieldableType& inh, const TypeContext& tctx) const;

		template<typename InheritableType>
		vm::InheritanceMetadata buildInheritanceMetadata(
			const InheritableType& inh, const TypeContext& tctx
		) const;

		/**
		 * @brief Validated a type, throws a builder error if type is invalid.
		 */
		void validateType(const TypeOfData& type) const;

		/**
		 * @brief Validated all types present in the context, throws a builder error if type is
		 * invalid.
		 */
		void validateTypes() const;

	public:
		void                                   addType(const TypeOfData& type);
		const StableTypeIdNameMap<TypeOfData>& getTypes() const;

		/**
		 * @brief Builds currently added types by building them.
		 */
		TypeContext build() const;
	};

	/**
	 * @brief Helper to compose bytecode instructions.
	 * It supports creating all available opcodes.
	 *
	 * Some operations support more arguments than their corresponding opcodes:
	 * * In case of `load` and `store`, third argument gets its own `ext` opcode.
	 */
	class InstructionBuilder {
		std::vector<vm::opargs::OpCodeArg> args;
		OpKind                             kind{};
		bool                               kind_set = false;

	public:
		InstructionBuilder() = default;
		InstructionBuilder(OpKind kind);

		template<class... Args>
		InstructionBuilder(OpKind kind, Args&&... args): InstructionBuilder(kind) {
			pushArgs(std::forward<Args>(args)...);
		}

		void setKind(OpKind kind);

		void pushArg(const vm::opargs::OpCodeArg& arg);

		template<class... Args>
		void pushArgs(Args&&... args) {
			(pushArg(std::forward<Args>(args)), ...);
		}

		[[nodiscard]] std::vector<Instruction> build() const;
	};

	using GlobalDataMap = StableTypeIdNameMap<GlobalData, GlobalDataID>;

	/**
	 * @brief Helper to compose bytecode functions.
	 */
	class FunctionBuilder {
		std::vector<Instruction> instructions{};
		Identifier               name;
		const TypeContext&       type_context;
		const GlobalDataMap&     globals;

	public:
		FunctionBuilder(Identifier name, const GlobalDataMap& globals, const TypeContext& types);

		/**
		 * @brief Adds instruction to the function.
		 */
		void addInstruction(const Instruction& instruction);

		/**
		 * @brief Builds and adds instruction(s) to the function.
		 */
		void addInstruction(const InstructionBuilder& instruction);

		[[nodiscard]] Function build() const;
	};
}
