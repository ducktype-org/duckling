/**
 * @file operation.hpp
 * @brief hold all operations (builtin, defaultable, custom)
 */

#pragma once

#include <exec/ctv.hpp>
#include <functional>
#include <typesystem/typesystem.hpp>
#include <base/maps.hpp>
#include <base/strongly_typed_id.hpp>

namespace operation {
	using Operation = std::function<exec::CTV(const std::vector<exec::CTV>&)>;

	void init();

	struct TypedOperation {
		const Operation        function;
		const ts::FunctionInfo signature;

		exec::CTV operator()(const std::vector<exec::CTV>& ctvs) const {
			RIFT_ASSERT(
				ctvs.size() == signature.getParameterTypes().size(),
				base::strConcat(
					"operation received incorrect number of arguments.",
					"expected ",
					signature.getParameterTypes().size(),
					" got ",
					ctvs.size()
				)
			);

			for (usize i = 0; i < ctvs.size(); i++) {
				// @TODO: this should check if types are compatible.
				// (possibly doing some conversion, or discarding consts?)
				RIFT_ASSERT(
					ctvs[i].type.getType() == signature.getParameterTypes()[i],
					"Argument has incorrect type"
				);
			}

			return function(ctvs);
		}
	};

	STRONG_TYPEDEF_ID(OperationID);

	using OperationMap = base::VectorMap<OperationID, TypedOperation>;


	enum class Defaultable {
		Equality,
		Compare,
		Assign,
		ConstructEmpty,
		ConstructFull,
	};


	using DefaultsMap = base::Map<std::pair<Defaultable, ts::TypeInfo>, OperationID>;

	TypedOperation getOperation(OperationID id);

	bool existsOperation(OperationID id);

	OperationID addOperation(const TypedOperation& operation);

	OperationID addDefault(Defaultable kind, ts::TypeInfo type, const TypedOperation& operation);

	TypedOperation& getDefault(Defaultable kind, ts::TypeInfo type);

	OperationID getIdDefault(Defaultable kind, ts::TypeInfo type);

	struct Call {
		const operation::OperationID op;
		const ts::TypeDesc<>         type;
		const usize                  offset;
		const usize                  size;

		exec::CTV operator()(const std::vector<exec::CTV>& ctvs) const {
			return getOperation(op)(ctvs);
		}
	};

	using Calls = std::vector<Call>;
}
