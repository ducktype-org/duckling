#pragma once

#include "allocator/block_data.hpp"
#include "allocator/dummy_allocator.hpp"
#include "allocator/heap_allocator.hpp"
#include "block.hpp"
#include "pointer.hpp"
#include "thread_stack.hpp"

#include <base/collections/maps.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/bytecode/constant_value_fd.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/utils/interpret.hpp>

#include <algorithm>
#include <deque>
#include <iostream>

namespace vm {
	/**
	 * @brief Helper structure that holds pointers to the global data buffer and global blocks buffer.
	 */
	template<typename EntryT, typename BlockT = BasicBlock<EntryT>>
	struct GlobalBufferPointers {
		EntryT*  data_buffer_base;    /// Base pointer to the global data buffer.
		BlockT** blocks_buffer_base;  /// Base pointer to the global blocks buffer.
	};

	/**
	 * @brief A memory module for a process.
	 * @note Memory is single threaded!
	 * All of process'es memory - thread stacks (thread local data) and global data is stored here.
	 */
	template<typename EntryT, typename BlockT = BasicBlock<EntryT>>
	class IMemory final {
	private:
		HeapAllocator<EntryT>  heap_allocator;
		DummyAllocator<EntryT> dummy_allocator;

		std::deque<BasicThreadStack<EntryT>> threads_frame_stacks;

		/// Buffer for the global data
		std::vector<EntryT> global_data_buffer{};

		/// Buffer for the global blocks
		std::vector<BlockT*> global_data_blocks{};

		/// If the global has been initialized (constructor has been called), then it is in this
		/// set. Otherwise, it is not.
		std::unordered_set<BlockID> initialized_globals{};

		// Here we use a simple recycling mechanism for blocks to avoid unnecessary allocations.
		// After the block is destroyed and the reference count drops to zero, instead of freeing
		// the memory, we mark the block as unused and add its ID to the free_ids list. Then when we
		// need to allocate a new block, we first check if there are any free IDs available. Blocks
		// are stored in a deque, so we can have pointers to them without worrying about
		// reallocation.
		std::deque<BlockT>  blocks   = {};
		std::deque<BlockID> free_ids = {};

		[[nodiscard]]
		Ref<BlockT> createBlock(BlockData<EntryT> data) {
			if constexpr (std::is_trivially_default_constructible_v<EntryT>) {
				std::memset(data.view.getBegin(), 0, data.view.size() * sizeof(EntryT));
			} else {
				std::fill(data.view.getBegin(), data.view.getBegin() + data.view.size(), EntryT{});
			}

			if (free_ids.empty()) {
				auto id = BlockID(blocks.size());
				blocks.emplace_back(id, data);
				return &blocks.back();
			} else {
				BlockID id = free_ids.back();
				free_ids.pop_back();
				blocks[usize(id)] = BlockT(id, data);
				return &blocks[static_cast<u64>(id)];
			}
		}

		/**
		 * @brief Erases the block object from the memory.
		 */
		void deleteBlock(Ref<BlockT> block) {
			if (!block->deallocated) throw exceptions::VMFoundMemoryLeakException();
			free_ids.push_back(block->id);
		}

		/**
		 * @brief Copies blocks from `block_src` to `block_dst`, going down the nested block
		 * hierarchy.
		 */
		void copyBlocksRecursively(Ref<BlockT> block_dst, Ref<BlockT> block_src) {
			runDataCopyConstructors(block_dst);
			for (auto nested: block_src->children_blocks) {
				setNestedViewBlock(block_dst, nested.first, nested.second->data.element_type);
				copyBlocksRecursively(block_dst->children_blocks[nested.first], nested.second);
			}
		}

		/**
		 * @brief Copies blocks from `block_src` to `block_dst`, going down the nested block
		 * hierarchy.
		 * @note Moving here means no data copy-constructors are called.
		 */
		void moveBlocksRecursively(Ref<BlockT> block_dst, Ref<BlockT> block_src) {
			for (auto nested: block_src->children_blocks) {
				setNestedViewBlock(block_dst, nested.first, nested.second->data.element_type);
				moveBlocksRecursively(block_dst->children_blocks[nested.first], nested.second);
			}
		}

		/**
		 * @brief Moves `entry_count` entries pointed-to by `src` to `dst`.
		 * Moving here means data copy-constructors of the moved object are not invoked.
		 * The objects that are in the "suffix" are destructed.
		 * @note Frees *block_dst's nested blocks whose offsets would not fit
		 * inside the new memory area.
		 * @note These blocks must be of a dynamic table type.
		 */
		void moveBlockDataAndEraseSuffix(Ref<BlockT> dst, Ref<BlockT> src, usize entry_count) {
			// Free all child blocks on suffix.
			auto& dst_child_blocks = dst->children_blocks;
			for (auto iter = dst_child_blocks.lower_bound(0); iter != dst_child_blocks.end();
				 iter      = dst_child_blocks.erase(iter)) {
				freeBlockData(iter->second);
			}

			// Copy the child blocks.
			auto& src_child_blocks = src->children_blocks;
			for (auto iter = src_child_blocks.lower_bound(0);
				 iter != src_child_blocks.end() && iter->first < entry_count;
				 ++iter) {
				auto offset = iter->first;
				setNestedViewBlock(dst, offset, iter->second->data.element_type);
				moveBlocksRecursively(dst->children_blocks[offset], iter->second);
			}

			// Copy the data itself.
			// @note: We are not running destructors or copy-constructors
			// because the data being is "moved".
			if constexpr (std::is_trivially_copyable_v<EntryT>) {
				std::memcpy(
					dst->data.view.getBegin(), src->data.view.getBegin(), entry_count * sizeof(EntryT)
				);
			} else {
				std::copy(
					src->data.view.getBegin(),
					src->data.view.getBegin() + entry_count,
					dst->data.view.getBegin()
				);
			}
		}

		/**
		 * @brief Replaces the block data memory view with the new one,
		 * taking care of the children blocks.
		 *
		 * @param block The block to update the data view for.
		 * @param new_view The new view to set for the block.
		 */
	public:
		void updateBlockDataView(Ref<BlockT> block, base::TypedModRawView<EntryT> new_view) {
			usize expected_size = block->data.element_type->getSize().asInt();
			CORE_ASSERT(
				expected_size == new_view.size(),
				"New view size must match the block's type size"
			);
			base::TypedModRawView<EntryT> old_root_view = block->data.view;

			std::function<void(Ref<BlockT>)> update_block_data_recursively
				= [&](Ref<BlockT> current_block) -> void {
				base::TypedModRawView<EntryT> current_view      = current_block->data.view;
				auto offset_from_start = current_view.getBegin() - old_root_view.getBegin();
				current_block->data.view
					= { new_view.getBegin() + offset_from_start, current_view.size() };

				for (auto& child: current_block->children_blocks | std::views::values)
					update_block_data_recursively(child);
			};

			update_block_data_recursively(block);
		}
	private:

		/**
		 * @brief Executes destructors on individual objects that are in the block.
		 * @param block The block to source the data from.
		 */
		void runDataDestructors(Ref<BlockT> block) {
			iterateOverDataAndExecute(block, &IMemory::runObjectDestructor);
		}

		/**
		 * @brief Executes destructors on a range of objects, that lay next to each other.
		 */
		void runDataDestructors(base::TypedModRawView<EntryT> data, TypeCRef type) {
			iterateOverDataAndExecute(data, type, &IMemory::runObjectDestructor);
		}

		/**
		 * @brief Executes copy constructors on individual objects that are in the block.
		 * @param block The block to source the data from.
		 */
		void runDataCopyConstructors(Ref<BlockT> block) {
			iterateOverDataAndExecute(block, &IMemory::runObjectCopyConstructor);
		}

		/**
		 * @brief Executes copy constructors on a range of objects, that lay next to each other.
		 */
		void runDataCopyConstructors(base::TypedModRawView<EntryT> data, TypeCRef type) {
			iterateOverDataAndExecute(data, type, &IMemory::runObjectCopyConstructor);
		}

		/**
		 * @brief Iterates over each object in the block and calls the callback on it.
		 * @note The callback should not change the layout of the objects in the block.
		 * @param callback A function that will be called on each object.
		 */
		void iterateOverDataAndExecute(
			Ref<BlockT> block,
			void (IMemory::*callback)(base::TypedModRawView<EntryT> data, TypeCRef type)
		) {
			iterateOverDataAndExecute(block->data.view, block->data.element_type, callback);
		}

		/**
		 * @brief Iterates over each object in the `data` and calls the callback on it.
		 * @note The callback should not change the layout of the objects in the block.
		 * @note In opposition to `runObjectDestructor` and `runObjectCopyConstructor`, here `data`
		 * can represent multiple objects.
		 * @param callback A function that will be called on each object.
		 */
		void iterateOverDataAndExecute(
			base::TypedModRawView<EntryT> data,
			TypeCRef                      type,
			void (IMemory::*callback)(base::TypedModRawView<EntryT> data, TypeCRef type)
		) {
			if (type->getKind() != Type::Kind::DynamicTable) {
				// Only types other than dynamic_table can be next to each other.
				// Callback on the first object
				(this->*callback)(
					base::TypedModRawView<EntryT>{ data.getBegin(), type->getSize().asInt() }, type
				);
			}

			switch (type->getKind()) {
			case Type::Kind::Primitive:
			case Type::Kind::Function:
			case Type::Kind::Opaque:
			case Type::Kind::Variant:
			case Type::Kind::Pointer:
				break;
			case Type::Kind::DynamicTable:
			case Type::Kind::FixedSizeTable: {
				const auto inner_type = type->getInnerType().value();
				const auto inner_size = inner_type->getSize().asInt();
				for (usize begin = 0; begin < data.size(); begin += inner_size)
					(this->*callback)(
						base::TypedModRawView<EntryT>{ data.getBegin() + begin, inner_size },
						inner_type
					);
				break;
			}
			case Type::Kind::Data: {
				// Iterate over data's fields
				for (const auto& fields = **type->getFields(); auto [offset, tp]: fields)
					(this->*callback)(
						base::TypedModRawView<EntryT>{ data.getBegin() + offset.asInt(),
													   tp->getSize().asInt() },
						tp
					);
				break;
			}
			default:
				CORE_PANIC("Handling default");
			}

			if (type->getKind() != Type::Kind::DynamicTable) {
				// In case we were given a slice of a table with multiple objects of the same type
				// laying next to each other, then iterate over those as well. Here we start from
				// the second, since the first one was handled above
				for (auto next_item = type->getSize().asInt(); next_item < data.size();
					 next_item += type->getSize().asInt()) {
					iterateOverDataAndExecute(
						base::TypedModRawView<EntryT>{ data.getBegin() + next_item,
													   type->getSize().asInt() },
						type,
						callback
					);
				}
			}
		}

		/**
		 * @brief Based on data's type, performs destruction of the data.
		 * E.g. in case of a non-null pointer, decreases pointed block's reference count.
		 * @note `data` has to represent a single object, not multiple objects - e.g. it can't be a
		 * range of objects from a table, but it can be a single object from a table, or from
		 * somewhere else.
		 */
		void runObjectDestructor(base::TypedModRawView<EntryT> data, TypeCRef type) {
			if constexpr (std::is_same_v<EntryT, std::byte>) {
				switch (type->getKind()) {
				case Type::Kind::Pointer: {
					const auto ptr = safeReadPointerBytes<Pointer>(data.getBegin());
					destroyBlockReference(ptr);
					break;
				}
				case Type::Kind::Primitive:
				case Type::Kind::Function:
				case Type::Kind::Opaque:
				case Type::Kind::DynamicTable:
				case Type::Kind::FixedSizeTable:
				case Type::Kind::Data:
				case Type::Kind::Variant:
					// There is nothing to do with variant, data and tables, because the data should
					// be already deleted thanks to the nested blocks structure, that deletes the
					// nested block's data first.
					break;
				default:
					CORE_PANIC("Handling default");
				}
			}
		}

		/**
		 * @brief Based on data's type, performs copy-constructor of the data.
		 * E.g. in case of a non-null pointer, increases pointed block's reference count.
		 * @note `data` has to represent a single object, not multiple objects - e.g. it can't be a
		 * range of objects from a table, but it can be a single object from a table, or from
		 * somewhere else.
		 */
		void runObjectCopyConstructor(base::TypedModRawView<EntryT> data, TypeCRef type) {
			if constexpr (std::is_same_v<EntryT, std::byte>) {
				switch (type->getKind()) {
				case Type::Kind::Pointer: {
					const auto ptr = safeReadPointerBytes<Pointer>(data.getBegin());
					if_opt_some(ptr.block.toOpt(), block) increaseBlockRefcount(block);
					break;
				}
				case Type::Kind::Primitive:
				case Type::Kind::Function:
				case Type::Kind::Opaque:
				case Type::Kind::DynamicTable:
				case Type::Kind::FixedSizeTable:
				case Type::Kind::Data:
				case Type::Kind::Variant:
					// There is nothing to do with variant, data and tables, because the data should
					// be already copied thanks to the nested blocks structure, that deletes the
					// nested block's data first.
					break;
				default:
					CORE_PANIC("Handling default");
				}
			}
		}

	public:
		IMemory() = default;

		[[nodiscard]]
		Ref<BlockT> getBlock(BlockID id) {
			const auto block_index = static_cast<usize>(id);
			if (block_index >= blocks.size()) throw exceptions::VMOutOfBlockBoundsException();
			if (blocks[block_index].deallocated) throw exceptions::VMUseAfterFreeException();
			return &blocks[block_index];
		}

		// =================== Used by the process ===================

		/**
		 * @brief Validates the memory state.
		 * It can be thought of as a check that is executed after program's exit
		 * to determine the correctness of memory usage
		 * (and potentially bugs inside the VM itself as well).
		 * For the memory to be valid, all the blocks' referenceCount needs to be 0. This means
		 * no leaks, etc.
		 * @return True if memory was used correctly, false otherwise.
		 */
		bool validateMemoryState() const {
#define TEST_HERE(test)                                              \
	if (test) {                                                      \
		std::cerr << #test ", BlockID=" << block.id.asInt() << "\n"; \
		return false;                                                \
	}
			for (const auto& block: blocks) {
				TEST_HERE(block.refcount != 0)
				TEST_HERE(!block.deallocated)
			}
			return true;
		}

		/**
		 * @brief Frees all the global data
		 */
		void deinitGlobals() {
			try {
				// We are first freeing all the data and then decreasing the refcounts.
				// This is very important, because there might be links between the global
				// variables, and if we were to free them and decrease the refcount in the wrong
				// order we might throw a false-positive exception. This solution avoids this
				// problem.

				for (const auto& block_ptr: global_data_blocks) freeBlockData(Ref(block_ptr));

				for (const auto& block_ptr: global_data_blocks)
					decreaseBlockRefcount(Ref(block_ptr));
			} catch (exceptions::VMFoundMemoryLeakException&) {
				std::cerr << "Leak during global data deinitialization - e.g. there was a global "
							 "pointer to "
							 "data, that was not freed.\n";
				throw;
			}
		}

		struct GlobalBlocksConfig {
			std::vector<usize>    global_data_offsets;
			std::vector<usize>    global_blocks_idxs;
			std::vector<TypeCRef> global_types;
			Bytes                 total_global_data_size;
			usize                 global_count;
		};

		/**
		 * @brief Allocates the memory for the global variables based on the provided configuration
		 * and creates the blocks for them. Works in the incremental setting, so only the new global
		 * variables are created, and the existing ones are left unchanged. The VMProcess can call
		 * this function when new global variables are added to the program.
		 *
		 * Does not support shrinking of the global buffer or decreasing the global count,
		 * meaning it can only be used to add new global variables.
		 *
		 * @warning It may invalidate the pointers to the global buffer, after calling this function
		 * always update the obtained pointers.
		 *
		 * @param global_blocks The configuration for the global blocks to initialize.
		 * @return The new pointers to the global data buffer and global blocks buffer.
		 */
		GlobalBufferPointers<EntryT, BlockT> initializeNewGlobalBlocks(
			const GlobalBlocksConfig& global_blocks
		) {
			CORE_ASSERT(
				global_blocks.global_count >= global_data_blocks.size(),
				"Global blocks buffer cannot be shrunk"
			);
			CORE_ASSERT(
				global_blocks.total_global_data_size.asInt() >= global_data_buffer.size(),
				"Global data buffer cannot be shrunk"
			);

			global_data_buffer.resize(global_blocks.total_global_data_size.asInt());
			global_data_blocks.resize(global_blocks.global_count);

			for (usize i = 0; i < global_blocks.global_count; i++) {
				usize    block_idx = global_blocks.global_blocks_idxs[i];
				usize    off       = global_blocks.global_data_offsets[i];
				TypeCRef type      = global_blocks.global_types[i];
				usize    type_size = type->getSize().asInt();
				CORE_ASSERT(
					block_idx < global_data_blocks.size(),
					"Global block index is out of bounds of the global blocks buffer"
				);
				MRef<BlockT> block_ref = MRef(global_data_blocks[block_idx]);

				if (block_ref.toOpt().empty()) {
					CORE_ASSERT(
						off + type_size <= global_data_buffer.size(),
						std::format(
							"Trying to insert global data of size {}, at offset {}, but buffer "
							"size is "
							"only {}",
							type_size,
							off,
							global_data_buffer.size()
						)
					);

					Ref<BlockT> block = allocateDummy(type, global_data_buffer.data() + off);
					increaseBlockRefcount(block);
					global_data_blocks[block_idx] = block.get();
				} else {
					// We have to update the view of the data block
					updateBlockDataView(
						block_ref.toOpt().value(), { global_data_buffer.data() + off, type_size }
					);
				}
			}

			return getGlobalDataMemory();
		}

		// =================== Used by executor ===================

		auto initializeFrameStack() -> Ref<BasicThreadStack<EntryT>> {
			threads_frame_stacks.emplace_back();
			return &threads_frame_stacks.back();
		}

		/**
		 * @brief Get the pointers to the global data buffer and global blocks buffer, that can be
		 * used by the threads.
		 */
		GlobalBufferPointers<EntryT, BlockT> getGlobalDataMemory() {
			return { .data_buffer_base   = global_data_buffer.data(),
					 .blocks_buffer_base = global_data_blocks.data() };
		}

		auto allocateHeap(TypeCRef type) -> Ref<BlockT> {
			return createBlock(heap_allocator.allocate(type));
		}

		/**
		 * @brief Allocates a contiguous new block of memory for n elements of `type`'s inner type.
		 * @note Assumes that type is a dynamic table type.
		 */
		auto dynTableAllocateHeapN(TypeCRef type, u64 n) -> Ref<BlockT> {
			auto inner_type = type->getInnerType().value();
			return createBlock(heap_allocator.dynTableAllocateN(type, inner_type, n));
		}

		/**
		 * @brief Creates a block with externally managed data life-time.
		 */
		auto allocateDummy(TypeCRef type, Ref<EntryT> data_pointer) -> Ref<BlockT> {
			return createBlock(dummy_allocator.allocate(type, data_pointer));
		}

		/**
		 * @brief Dynamically reallocates block data.
		 * @note Assumes that type is a dynamic table type and reallocates it to
		   a table of size n with elements of type equal to type's inner type.
		 */
		auto dynTableReallocateBlockDataN(Ref<BlockT> block, u64 n) -> void {
			auto              tbl_type       = block->data.element_type;
			auto              inner_type     = tbl_type->getInnerType().value();
			BlockData<EntryT> new_block_data = heap_allocator.dynTableAllocateN(tbl_type, inner_type, n);
			auto              old_view_size  = block->data.view.size();
			auto              new_view_size  = new_block_data.view.size();

			BlockT mock_block{ BlockID{ 0 }, new_block_data };

			moveBlockDataAndEraseSuffix(&mock_block, block, std::min(old_view_size, new_view_size));

			heap_allocator.deallocate(&block->data);

			block->data = mock_block.data;
		}

		/**
		 * @brief Frees block's data, but not the block structure itself.
		 * For the block to be freed, use deleteBlock.
		 */
		void freeBlockData(Ref<BlockT> block) {
			for (const auto child: block->children_blocks | std::views::values) freeBlockData(child);

			runDataDestructors(block);

			// Parents reference their children so that they don't disappear on someone's pointer
			// destruction.
			if (block->parent) {
				block->deallocated = true;
				decreaseBlockRefcount(block);
			} else {
				block->data.allocator->deallocate(&block->data);
				block->deallocated = true;
			}
		}

		/**
		 * @brief Checks if the global variable in the block has been initialized (constructor has
		 * been called).
		 *
		 * Should be called from a place where the runtime initialization is happening.
		 * If the global variable has no constructor this function shouldn't be called.
		 *
		 * @param global_block The block of the global variable to check.
		 * @return True if the global variable has been initialized, false otherwise.
		 */
		bool isGlobalInitialized(Ref<BlockT> global_block) {
			return initialized_globals.contains(global_block->id);
		}

		/**
		 * @brief Set the global as initialized (the constructor has been called).
		 *
		 * Should be called from a place where the runtime initialization is happening.
		 * If the global variable has no constructor this function shouldn't be called.
		 *
		 * @param global_block The block of the global variable to set as initialized.
		 */
		void setGlobalInitialized(Ref<BlockT> global_block) {
			initialized_globals.insert(global_block->id);
		}


		void initializeBlockFromConstValue(Ref<Block> block, const code::ConstantValue& const_value);

		/**
		 * @brief Returns a view of block's data
		 */
		[[nodiscard]] constexpr __attribute__((always_inline)) auto getBlockViewUnsafe(
			Ref<BlockT> block
		) -> base::TypedModRawView<EntryT> {
			return block->data.view;
		}

		// =================== Variant operations ===================

		/**
		 * @brief Gets the nested block from block at offset.
		 * @note Parent pointer also stores offset within the parent block where to take the
		 * nested view block from.
		 */
		static MRef<BlockT> getNestedViewBlock(Ref<BlockT> parent_block, u64 offset, TypeCRef type) {
			if_opt_some(parent_block->children_blocks.atMaybe(offset), nested) {
				if ((*nested)->data.element_type == type) return *nested;
			}
			return nullptr;
		}

		/**
		 * @brief Creates new block at position.
		 * @note Parent pointer also stores offset within the parent block where to create the new
		 * nested view block.
		 */
		void setNestedViewBlock(Ref<BlockT> parent_block, u64 offset, TypeCRef type) {
			auto& children = parent_block->children_blocks;
			if_opt_some(children.atMaybe(offset), nested) {
				freeBlockData(*nested);
				children.erase(offset);
			}

			auto block_data         = parent_block->data;
			block_data.element_type = type;
			u64  entry_count        = type->getSize().asInt();
			if (parent_block->deallocated) throw exceptions::VMUseAfterFreeException();
			if (offset + entry_count > parent_block->data.view.size())
				throw exceptions::VMOutOfBlockBoundsException();
			block_data.view = { parent_block->data.view.getBegin() + offset, entry_count };

			auto new_block    = createBlock(block_data);  // @note createBlock nulls them bytes
			new_block->parent = parent_block;
			increaseBlockRefcount(new_block);  // so that the block does not disappear accidentally
			children.put(offset, new_block);
		}

		// =================== Block operations ===================

		[[nodiscard]]
		static auto getBlockType(Ref<BlockT> block) -> TypeCRef {
			return block->data.element_type;
		}

		// ======================== Pointers ========================

		static void increaseBlockRefcount(Ref<BlockT> block) { block->refcount++; }

		void decreaseBlockRefcount(Ref<BlockT> block) {
			CORE_ASSERT(
				block->refcount > 0, "Deleting an unreferenced block"
			);  // This should never be possible, even in a faulty program
			if (--block->refcount == 0) deleteBlock(block);
		}

		[[nodiscard]]
		static auto newBlockReference(Ref<Block> block, u64 offset) -> Pointer
			requires std::is_same_v<EntryT, std::byte>
		{
			increaseBlockRefcount(block);
			return { block, offset };
		}

		[[nodiscard]]
		static constexpr __attribute__((always_inline)) auto getPointerData(
			Pointer pointer, u64 entry_count
		) -> base::TypedModRawView<std::byte>
			requires std::is_same_v<EntryT, std::byte>
		{
			if (pointer.block == nullptr) throw exceptions::VMNullPointerAccessException();
			if (pointer.block->deallocated) throw exceptions::VMUseAfterFreeException();
			if (pointer.offset + entry_count > pointer.block->data.view.size())
				throw exceptions::VMOutOfBlockBoundsException();
			return { pointer.block->data.view.getBegin() + pointer.offset, entry_count };
		}

		/**
		 * @brief Copies data pointed-to by `src` to `dst`.
		 * @note Assumes that the size of `type` is known at compile time and (implicitly)
		 * that it is the type of the blocks pointed-to by `dst` and `src`.
		 */
		auto copyPointedData(Pointer dst, Pointer src, TypeCRef type) -> void
			requires std::is_same_v<EntryT, std::byte>
		{
			if (dst.isNull() || src.isNull()) throw exceptions::VMNullPointerCopyException();

			const usize entry_count = type->getSize().asInt();

			// Free child blocks.
			auto& dst_child_blocks = dst.getBlock()->children_blocks;
			for (auto iter = dst_child_blocks.lower_bound(dst.offset);
				 iter != dst_child_blocks.end() && iter->first < dst.offset + entry_count;
				 iter = dst_child_blocks.erase(iter)) {
				freeBlockData(iter->second);
			}

			const auto dst_view = getPointerData(dst, entry_count);
			const auto src_view = getPointerData(src, entry_count);
			runDataDestructors(dst_view, type);

			// Copy the child blocks
			auto& src_child_blocks = src.getBlock()->children_blocks;
			for (auto iter = src_child_blocks.lower_bound(src.offset);
				 iter != src_child_blocks.end() && iter->first < src.offset + entry_count;
				 ++iter) {
				auto offset = dst.offset + iter->first - src.offset;
				setNestedViewBlock(dst.getBlock(), offset, iter->second->data.element_type);
				copyBlocksRecursively(dst.getBlock()->children_blocks[offset], iter->second);
			}

			// Copy the data itself
			std::memcpy(dst_view.getBegin(), src_view.getBegin(), entry_count);
			runDataCopyConstructors(dst_view, type);
		}

		auto destroyBlockReference(Pointer pointer) -> void
			requires std::is_same_v<EntryT, std::byte>
		{
			if_opt_some(pointer.block.toOpt(), block) { decreaseBlockRefcount(block); }
		}

		auto updatePointerAssignment(Pointer dst, Pointer src) -> Pointer
			requires std::is_same_v<EntryT, std::byte>
		{
			if (dst.block != src.block) {
				destroyBlockReference(dst);
				if_opt_some(src.block.toOpt(), block) { increaseBlockRefcount(block); }
			}
			return src;
		}

		// ======================== Requests ========================

		[[nodiscard]]
		auto requestBlockID(Ref<BlockT> block) -> BlockID {
			return block->id;
		}

		[[nodiscard]]
		auto requestBlockData(BlockID id) -> base::TypedModRawView<const EntryT> {
			auto b = getBlock(id);
			return { b->data.view.getBegin(), b->data.view.size() };
		}

		[[nodiscard]]
		auto requestBlockType(BlockID id) -> TypeCRef {
			return getBlock(id)->data.element_type;
		}
	};

	using Memory = IMemory<std::byte>;
	using PointerGeneric = Pointer;
	using GlobalBufferPointersGeneric = GlobalBufferPointers<std::byte>;
}
