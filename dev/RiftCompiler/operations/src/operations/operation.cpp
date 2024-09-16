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

	TypedOperation getOperation(OperationID id) { return getOperations()[id]; }

	bool existsOperation(OperationID id) { return getOperations().contains(id); }

	OperationID addOperation(const TypedOperation& operation) {
		auto id = OperationID::next();
		getOperations().put(id, operation);
		return id;
	}

	OperationID addDefault(Defaultable kind, ts::TypeInfo type, const TypedOperation& operation) {
		// @TODO: One day add checking if the signature is ok
		if (getDefaults().contains({ kind, type }))
			throw base::LogicError("Redeclaration of default operation is not legal.");

		OperationID id = OperationID::next();
		getOperations().put(id, operation);
		getDefaults().put({ kind, type }, id);
		return id;
	}

	TypedOperation& getDefault(Defaultable kind, ts::TypeInfo type) {
		OperationID id = getIdDefault(kind, type);
		return getOperations()[id];
	}

	OperationID getIdDefault(Defaultable kind, ts::TypeInfo type) {
		return getDefaults().at({ kind, type });
	}
}
