#include <base/collections/object_pool.hpp>

#include <vm/core/safe/memory/block.hpp>

/**
 * @brief Compile-time gate test for issue #2221.
 *
 * Verifies that StableObjectPool<vm::Block> instantiates with the
 * exact template parameters Memory will use after the refactoring:
 *   - ObjID = u64
 *   - SHOULD_RECYCLE = true
 *   - PASS_ID_TO_CONSTRUCTOR = true
 *
 * If this file compiles, the Block type satisfies all StableObjectPool
 * requirements (constructible with (u64, BlockData), move-assignable).
 */
inline constexpr bool BLOCK_COMPATIBLE_WITH_STABLE_OBJECT_POOL
	= sizeof(base::StableObjectPool<vm::Block, u64, true, true>) > 0;

int main() { return BLOCK_COMPATIBLE_WITH_STABLE_OBJECT_POOL ? 0 : 1; }
