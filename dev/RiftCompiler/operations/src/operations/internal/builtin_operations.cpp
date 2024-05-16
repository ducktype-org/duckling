#include "builtin_operations.hpp"
#include "../create_default.hpp"
#include <exec/exec_builtin.hpp>

namespace operation {
	namespace {
		// @TODO: maybe use some of the tools that are created in the operators branch.

		template<typename T, usize SIZE>
		void addIntegral() {
			static_assert(SIZE == 8 * (sizeof(T)));

			auto int_type = query::entryPoint<ts::QueryIntegralType>({ SIZE });

			operation::Operation comp_int            = exec::compareBuiltin<T>;
			operation::Operation eq_int              = exec::equalityBuiltin<T>;
			operation::Operation assign_int          = exec::assignBuiltin<T>;
			operation::Operation construct_empty_int = exec::emptyBuiltin<T>;

			operation::TypedOperation comp_int_t = comparison(comp_int, int_type);
			operation::TypedOperation eq_int_t   = equality(eq_int, int_type);

			operation::TypedOperation assign_int_t = assign(assign_int, int_type);

			operation::TypedOperation construct_empty_int_t
				= construct(construct_empty_int, int_type);

			operation::addDefault(operation::Defaultable::Assign, int_type, assign_int_t);
			operation::addDefault(operation::Defaultable::Equality, int_type, eq_int_t);
			operation::addDefault(operation::Defaultable::Compare, int_type, comp_int_t);
			operation::addDefault(
				operation::Defaultable::ConstructEmpty, int_type, construct_empty_int_t
			);
		}
	}

	void addBuiltinOperations() {
		addIntegral<int8_t, 8>();
		addIntegral<i16, 16>();
		addIntegral<i32, 32>();
		addIntegral<i64, 64>();
		addIntegral<i128, 128>();
	}

}
