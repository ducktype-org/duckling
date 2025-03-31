/**
 * @file operation.hpp
 * @brief hold all operations (builtin, defaultable, custom)
 */

#pragma once

#include <functional>

#include <base/maps.hpp>
#include <base/strongly_typed_id.hpp>

#include <exec/ctv.hpp>
#include <typesystem/typesystem.hpp>

namespace operation {
	using Operation = std::function<exec::CTV(const std::vector<exec::CTV>&)>;

	void init();

	struct TypedOperation {
		const Operation        function;
		const ts::FunctionInfo signature;

		exec::CTV operator()(const std::vector<exec::CTV>& ctvs) const {
			CORE_ASSERT(
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
				CORE_ASSERT(
					ctvs[i].type.getType() == signature.getParameterTypes()[i],
					"Argument has incorrect type"
				);
			}

			return function(ctvs);
		}
	};

	STRONG_TYPEDEF_ID(OperationId);

	using OperationMap = base::VectorMap<OperationId, TypedOperation>;


	enum class Defaultable {
		Equality,
		Compare,
		Assign,
		ConstructEmpty,
		ConstructFull,
	};


	using DefaultsMap = base::Map<std::pair<Defaultable, ts::TypeInfo>, OperationId>;

	TypedOperation getOperation(OperationId id);

	bool existsOperation(OperationId id);

	OperationId addOperation(const TypedOperation& operation);

	OperationId addDefault(Defaultable kind, ts::TypeInfo type, const TypedOperation& operation);

	TypedOperation& getDefault(Defaultable kind, ts::TypeInfo type);

	OperationId getIdDefault(Defaultable kind, ts::TypeInfo type);

	struct Call {
		const operation::OperationId op;
		const ts::TypeDesc<>         type;
		const usize                  offset;
		const usize                  size;

		exec::CTV operator()(const std::vector<exec::CTV>& ctvs) const {
			return getOperation(op)(ctvs);
		}
	};

	using Calls = std::vector<Call>;
}
