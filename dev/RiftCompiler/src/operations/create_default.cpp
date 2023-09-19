#include "create_default.hpp"

#include <exec/ctv.hpp>
#include <exec/exec_default.hpp>
#include <exec/vtable_creation.hpp>
#include <operations/operation.hpp>
#include <typesystem/class_types.hpp>
#include <typesystem/type_desc.hpp>
#include <typesystem/typesystem.hpp>

namespace operation {

	namespace op = operation;

	namespace internal {
		Calls collectCallsClass(ts::ClassInfo class_info, operation::Defaultable kind) {
			Calls res;

			for (const auto& member_data : class_info.members()) {
				auto op = operation::getIdDefault(kind, member_data.desc.getType());

				Call curr(
					op, member_data.desc, member_data.offset, member_data.desc.getType().getSize());
				res.push_back(curr);
			}

			for (const auto& parent_data : class_info.basicParents()) {
				auto op = operation::getIdDefault(kind, parent_data.info);

				Call curr(op,
				          ts::TypeDesc<>(parent_data.info),
				          parent_data.offset,
				          parent_data.info.getSize());
				res.push_back(curr);
			}

			return res;
		}

		Calls collectCallsTuple(ts::TupleInfo tuple_info, op::Defaultable kind) {
			usize count = tuple_info.getUnderlyingTypes().size();

			Calls res;

			for (i32 i = 0; i < count; i++) {
				auto [type_desc, offset] = tuple_info.getMember(i);

				auto op = operation::getIdDefault(kind, type_desc.getType());

				Call curr(op, type_desc, offset, type_desc.getType().getSize());
				res.push_back(curr);
			}

			return res;
		}

		Calls collectCalls(ts::TypeInfo type_info, op::Defaultable kind) {
			switch (type_info.getKind()) {
			case ts::Kind::Class:
				return collectCallsClass(type_info, kind);
			case ts::Kind::Tuple:
				return collectCallsTuple(type_info, kind);
			default:
				RIFT_PANIC("Cannot collect calls for type other than class or tuple.");
			}
		}

	}

	operation::TypedOperation createDefaultEquality(ts::TypeInfo type_info) {
		Calls calls = internal::collectCalls(type_info, operation::Defaultable::Equality);


		operation::Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 2, "Comparison should receive two values.")

			return exec::defaultEquality(type_info, calls, input[0], input[1]);
		};

		// @TODO: Add flags (const) to the type desc in all the default operations
		ts::FunctionInfo sig
			= ts::FunctionInfo::create({ { type_info }, { type_info } }, ts::BoolInfo::create());

		return operation::TypedOperation{ op, sig };
	}

	operation::TypedOperation createDefaultVirtualEquality(ts::ClassInfo class_info) {
		operation::Operation op = [class_info](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 2, "Comparison should receive two values.")

			return exec::defaultEqualityVirtual(class_info, input[0], input[1]);
		};

		// @TODO: Add flags (const) to the type desc in all the default operations
		ts::FunctionInfo sig
			= ts::FunctionInfo::create({ { class_info }, { class_info } }, ts::BoolInfo::create());

		return operation::TypedOperation{ op, sig };
	}

	operation::TypedOperation createDefaultComparison(ts::TypeInfo type_info) {
		Calls calls = internal::collectCalls(type_info, operation::Defaultable::Compare);


		operation::Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 2, "Comparison should receive two values.")
			return exec::defaultCompare(type_info, calls, input[0], input[1]);
		};

		ts::FunctionInfo sig = ts::FunctionInfo::create({ { type_info }, { type_info } },
		                                                ts::IntegralInfo::create(8));

		return { op, sig };
	}

	operation::TypedOperation createDefaultAssign(ts::TypeInfo type_info) {
		Calls calls = internal::collectCalls(type_info, operation::Defaultable::Assign);

		operation::Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 2, "Assigment should receive two values.")
			return exec::defaultAssign(type_info, calls, input[0], input[1]);
		};

		ts::FunctionInfo sig
			= ts::FunctionInfo::create({ { type_info }, { type_info } }, type_info);

		return { op, sig };
	}

	operation::TypedOperation createDefaultConstructEmpty(ts::TypeInfo type_info) {
		Calls calls = internal::collectCalls(type_info, operation::Defaultable::ConstructEmpty);

		operation::Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			RIFT_ASSERT(input.size() == 1, "Empty constructor should receive one CTV.")
			return exec::defaultConstructEmpty(type_info, calls, input[0]);
		};

		ts::FunctionInfo sig = ts::FunctionInfo::create({ { type_info } }, type_info);

		return { op, sig };
	}

	operation::TypedOperation createDefaultConstructFull(ts::TypeInfo type_info) {
		// @TODO: how should members be constructed?

		Calls calls = internal::collectCalls(type_info, operation::Defaultable::Assign);

		operation::Operation op = [type_info, calls](const std::vector<exec::CTV>& input) {
			return exec::defaultConstructFull(type_info, calls, input);
		};

		std::vector<ts::TypeDesc<>> args = { type_info };

		for (usize i = 0; i < calls.size(); i++) args.push_back(calls[i].type);

		ts::FunctionInfo sig = ts::FunctionInfo::create(args, type_info);

		return { op, sig };
	}

}
