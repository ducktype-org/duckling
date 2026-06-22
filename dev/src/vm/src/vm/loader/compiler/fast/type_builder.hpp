#pragma once
#include <vm/bytecode/validator/valid_type/type_map.hpp>
#include <vm/core/fast/program/type.hpp>

namespace vm::loader::compiler::fast {
	/**
	 * @brief Incrementally builds fast-mode Type objects for validated types not yet present in
	 * @p type_collection, first declaring them and then defining their kinds.
	 *
	 * @param type_collection In/out parameter: the fast-mode type collection to extend. Existing
	 * entries are kept; types from @p type_ctx beyond the current size are appended.
	 * @param type_ctx The validated type map to source new type definitions from.
	 */
	void rebuildFastTypeCollection(
		Ref<vm::fast::TypeCollection>             type_collection,
		const vm::code::valid_type::ValidTypeMap& type_ctx
	);
}
