#include <exec/ctv.hpp>
#include <exec/exec_default.hpp>
#include <exec/vtable_creation.hpp>

#include <operations/operation.hpp>
#include <typesystem/class_types.hpp>
#include <typesystem/type_desc.hpp>
#include <typesystem/typesystem.hpp>

#include "create_default.hpp"

namespace operation {

	namespace op = operation;

	namespace internal {
		Calls collectCalls(const ts::TypeInfo type_info, const Defaultable kind) {
			switch (type_info.getKind()) {
			case ts::Kind::Class:
				RIFT_PANIC("Unimplemented. See file history for details.");
			case ts::Kind::Tuple:
				RIFT_PANIC("Unimplemented. See file history for details.");
			default:
				(void) kind;
				RIFT_PANIC("Cannot collect calls for type other than class or tuple.");
			}
		}

	}

	TypedOperation createDefaultEquality(ts::TypeInfo type_info) {
		Calls calls = internal::collectCalls(type_info, Defaultable::Equality);


		const Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 2, "Comparison should receive two values.");

			return defaultEquality(type_info, calls, input[0], input[1]);
		};

		// @TODO: Add flags (const) to the type desc in all the default operations
		const ts::FunctionInfo sig = ts::FunctionInfo::create(
			{ { type_info }, { type_info } }, query::queryEntryPoint<ts::QueryBoolType>({})
		);

		return TypedOperation{ op, sig };
	}

	TypedOperation createDefaultVirtualEquality(ts::ClassInfo class_info) {
		const Operation op = [class_info](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 2, "Comparison should receive two values.");

			return defaultEqualityVirtual(class_info, input[0], input[1]);
		};

		// @TODO: Add flags (const) to the type desc in all the default operations
		const ts::FunctionInfo sig = ts::FunctionInfo::create(
			{ { class_info }, { class_info } }, query::queryEntryPoint<ts::QueryBoolType>({})
		);

		return TypedOperation{ op, sig };
	}

	TypedOperation createDefaultComparison(ts::TypeInfo type_info) {
		Calls calls{};  // internal::collectCalls(type_info, Defaultable::Compare);


		const Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 2, "Comparison should receive two values.");
			return defaultCompare(type_info, calls, input[0], input[1]);
		};

		const ts::FunctionInfo sig = ts::FunctionInfo::create(
			{ { type_info }, { type_info } }, query::queryEntryPoint<ts::QueryIntegralType>({ 8 })
		);

		return { op, sig };
	}

	TypedOperation createDefaultAssign(ts::TypeInfo type_info) {
		Calls calls{};  // internal::collectCalls(type_info, Defaultable::Assign);

		const Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 2, "Assigment should receive two values.");
			return defaultAssign(type_info, calls, input[0], input[1]);
		};

		const ts::FunctionInfo sig
			= ts::FunctionInfo::create({ { type_info }, { type_info } }, type_info);

		return { op, sig };
	}

	TypedOperation createDefaultConstructEmpty(ts::TypeInfo type_info) {
		Calls calls{};  // internal::collectCalls(type_info, Defaultable::ConstructEmpty);

		const Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 1, "Empty constructor should receive one CTV.");
			return defaultConstructEmpty(type_info, calls, input[0]);
		};

		const ts::FunctionInfo sig = ts::FunctionInfo::create({ { type_info } }, type_info);

		return { op, sig };
	}

	TypedOperation createDefaultConstructFull(ts::TypeInfo type_info) {
		// @TODO: how should members be constructed?

		Calls calls{};  // internal::collectCalls(type_info, Defaultable::Assign);

		const Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			return defaultConstructFull(type_info, calls, input);
		};

		std::vector<ts::TypeDesc<>> args = { type_info };

		for (auto& [op_id, type, offset, size]: calls) args.push_back(type);

		const ts::FunctionInfo sig = ts::FunctionInfo::create(args, type_info);

		return { op, sig };
	}

}
