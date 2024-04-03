#include "operation.hpp"
#include "internal/builtin_operations.hpp"

namespace operation {

	void init() {
		static bool was_init = false;
		if (was_init) return;
		was_init = true;

		addBuiltinOperations();
	}

	namespace {
		OperationMap& getOperations() {
			static OperationMap operations;
			return operations;
		}

		DefaultsMap& getDefaults() {
			static DefaultsMap defaults;
			return defaults;
		}
	}

	TypedOperation getOperation(OperationId id) { return getOperations()[id]; }

	bool existsOperation(OperationId id) { return getOperations().contains(id); }

	OperationId addOperation(const TypedOperation& operation) {
		auto id = OperationId::next();
		getOperations().put(id, operation);
		return id;
	}

	OperationId addDefault(Defaultable kind, ts::TypeInfo type, const TypedOperation& operation) {
		// @TODO: One day add checking if the signature is ok
		if (getDefaults().contains({ kind, type }))
			throw base::LogicError("Redeclaration of default operation is not legal.");

		OperationId id = OperationId::next();
		getOperations().put(id, operation);
		getDefaults().put({ kind, type }, id);
		return id;
	}

	TypedOperation& getDefault(Defaultable kind, ts::TypeInfo type) {
		OperationId id = getIdDefault(kind, type);
		return getOperations()[id];
	}

	OperationId getIdDefault(Defaultable kind, ts::TypeInfo type) {
		return getDefaults().at({ kind, type });
	}
}
