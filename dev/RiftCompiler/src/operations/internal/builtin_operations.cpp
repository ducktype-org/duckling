#include "builtin_operations.hpp"
#include "../create_default.hpp"
#include <exec/exec_builtin.hpp>

namespace operation {
	namespace {
		// @TODO: maybe use some of the tools that are created in the operators branch.

		template<typename T, size_t SIZE>
		void addIntegral() {
			static_assert(SIZE == 8 * (sizeof(T)));

			auto int_type = ts::IntegralInfo::create(SIZE);
			ts::TypeDesc<> int_desc{int_type};
			ts::TypeDesc<> bool_desc{ts::BoolInfo::create()};

			operation::Operation comp_int = exec::compareBuiltin<T>;
			operation::Operation eq_int = exec::equalityBuiltin<T>;
			operation::Operation assign_int = exec::assignBuiltin<T>;
			operation::Operation construct_empty_int = exec::emptyBuiltin<T>;

			operation::TypedOperation comp_int_t = comparison(comp_int, int_desc);
			operation::TypedOperation eq_int_t = equality(eq_int, int_desc);

			operation::TypedOperation assign_int_t = assign(assign_int, int_desc);

			operation::TypedOperation construct_empty_int_t =
				construct(construct_empty_int, int_desc);

			operation::addDefault(operation::Defaultable::Assign, int_type, assign_int_t);
			operation::addDefault(operation::Defaultable::Equality, int_type, eq_int_t);
			operation::addDefault(operation::Defaultable::Compare, int_type, comp_int_t);
			operation::addDefault(
				operation::Defaultable::ConstructEmpty, int_type, construct_empty_int_t);
		}
	}


	void addBuiltinOperations() {
		addIntegral<int8_t, 8>();
		addIntegral<int16_t, 16>();
		addIntegral<int32_t, 32>();
		addIntegral<int64_t, 64>();
		addIntegral<__int128, 128>();
	}

}
