#pragma once

#include "allocator/block_data.hpp"
#include "allocator/dummy_allocator.hpp"
#include "allocator/heap_allocator.hpp"
#include "block.hpp"
#include "pointer.hpp"
#include "thread_stack.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/object_pool.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/bytecode/constant_value_fd.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/utils/interpret.hpp>

#include <algorithm>
#include <deque>

namespace vm {
	/**
	 * @brief Helper structure that holds pointers to the global data buffer and global blocks buffer.
	 */
	template<typename EntryT, typename BlockT = GenericBlock<EntryT>>
	struct GlobalBufferPointers {
		EntryT*  data_buffer_base;    /// Base pointer to the global data buffer.
		BlockT** blocks_buffer_base;  /// Base pointer to the global blocks buffer.
	};

	/**
	 * @brief A memory module for a process.
	 * @note Memory is single threaded!
	 * All of process'es memory - thread stacks (thread local data) and global data is stored here.
	 */
	template<typename EntryT, typename BlockT = GenericBlock<EntryT>>
	class GenericMemory final {
	private:
		HeapAllocator<EntryT>  heap_allocator;
		DummyAllocator<EntryT> dummy_allocator;

		std::deque<GenericThreadStack<EntryT>> threads_frame_stacks;

		/// Buffer for the global data
		std::vector<EntryT> global_data_buffer{};

		/// Buffer for the global blocks
		std::vector<BlockT*> global_data_blocks{};

		/// If the global has been initialized (constructor has been called), then it is in this
		/// set. Otherwise, it is not.
		std::unordered_set<BlockID> initialized_globals{};

		base::StableObjectPool<BlockT, BlockID, true, true> blocks_pool;

		/**
		 * @brief Registers a block over `data` without touching the data itself.
		 * @note Only for data that already holds a live value, see `adoptDummy`.
		 */
		[[nodiscard]]
		Ref<BlockT> adoptBlock(BlockData<EntryT> data) {
			auto id = blocks_pool.add(data);
			return blocks_pool.get(id);
		}

		[[nodiscard]]
		Ref<BlockT> createBlock(BlockData<EntryT> data) {
			if constexpr (std::is_trivially_default_constructible_v<EntryT>)
				std::memset(data.view.getBegin(), 0, data.view.size() * sizeof(EntryT));
			else
				std::fill(data.view.getBegin(), data.view.getBegin() + data.view.size(), EntryT{});

			return adoptBlock(data);
		}

		[[nodiscard]]
		Ref<BlockT> getBlock(BlockID id) {
			if (auto maybe_block = blocks_pool.maybeGet(id)) {
				Ref<BlockT> block_ref = *maybe_block;
				if (block_ref->deallocated) throw exceptions::VMUseAfterFreeException();
				return block_ref;
			}
			throw exceptions::VMOutOfBlockBoundsException();
		}

		/**
		 * @brief Erases the block object from the memory.
		 */
		void deleteBlock(Ref<BlockT> block) {
			if (!block->deallocated) throw exceptions::VMFoundMemoryLeakException();
			blocks_pool.remove(block->id);
		}

		/**
		 * @brief Copies blocks from `block_src` to `block_dst`, going down the nested block
		 * hierarchy.
		 */
		void copyBlocksRecursively(Ref<BlockT> block_dst, Ref<BlockT> block_src) {
			// @note using SRC, because setNestedViewBlock zeroes the entries
			runDataCopyConstructors(block_src);
			for (auto nested: block_src->children_blocks) {
				setNestedViewBlock(block_dst, nested.first, nested.second->data.element_type);
				copyBlocksRecursively(block_dst->children_blocks[nested.first], nested.second);
			}
		}

		/**
		 * @brief Changes `BlockData` object that is used underneath a block.
		 * It does so by moving objects from block's data to new_data.
		 * This means data copy-constructors of the moved objects are not invoked.
		 * @note Frees *block's nested blocks whose offsets would not fit
		 * inside the `new_data` memory area - the objects that are in the suffix are destructed,
		 * so the suffix must not hold moved-from objects.
		 * @note The part of `new_data` past the moved objects is cleared.
		 * @note The old data is deallocated with its own allocator.
		 */
		// @TODO: #3447 `children_blocks` is keyed by a byte offset while `entries_to_move` is an
		// entry count; the two are interchangeable only while `sizeof(EntryT) == 1`.
		void changeBlockData(Ref<BlockT> block, BlockData<EntryT> new_data) {
			BlockData<EntryT> old_data = block->data;
			usize entries_to_move      = std::min(old_data.view.size(), new_data.view.size());

			// Free the unfitting children blocks.
			auto& children_blocks = block->children_blocks;
			for (auto child_it = children_blocks.lower_bound(entries_to_move);
			     child_it != children_blocks.end();
			     child_it = children_blocks.erase(child_it)) {
				freeBlockData(child_it->second);
			}

			// Run the destructors - e.g. pointers don't have their own blocks, but need
			// destructing. The suffix has to hold live objects. Here we run the destructor for the
			// second time, but as different type. And this pass is shallow. First pass goes over
			// child blocks, eg variant data - a pointer. Then in the second pass we run the
			// destructor on variant type, which does nothing.
			if (entries_to_move < old_data.view.size())
				runDataDestructors(
					base::TypedModRawView<EntryT>(
						old_data.view.getBegin() + entries_to_move,
						old_data.view.size() - entries_to_move
					),
					old_data.element_type
				);

			if constexpr (std::is_trivially_copyable_v<EntryT>) {
				std::memcpy(
					new_data.view.getBegin(),
					old_data.view.getBegin(),
					entries_to_move * sizeof(EntryT)
				);
			} else {
				std::copy(
					old_data.view.getBegin(),
					old_data.view.getBegin() + entries_to_move,
					new_data.view.getBegin()
				);
			}

			if constexpr (std::is_trivially_default_constructible_v<EntryT>) {
				std::memset(
					new_data.view.getBegin() + entries_to_move,
					0,
					(new_data.view.size() - entries_to_move) * sizeof(EntryT)
				);
			} else {
				std::fill(
					new_data.view.getBegin() + entries_to_move,
					new_data.view.getBegin() + new_data.view.size(),
					EntryT{}
				);
			}

			updateBlockDataView(block, new_data.view);
			block->data = new_data;

			old_data.allocator->deallocate(&old_data);
		}

		/**
		 * @brief Replaces the block data memory view with the new one,
		 * taking care of the children blocks.
		 *
		 * @note The children keep their own sizes, only their base is rebased onto `new_view`.
		 *
		 * @param block The block to update the data view for.
		 * @param new_view The new view to set for the block.
		 */

	public:
		void updateBlockDataView(Ref<BlockT> block, base::TypedModRawView<EntryT> new_view) {
			const EntryT* old_root_begin = block->data.view.getBegin();

			auto rebase_children_recursively
				= [&](this const auto& self, Ref<BlockT> current_block) -> void {
				for (auto& child: current_block->children_blocks | std::views::values) {
					const auto offset_from_start = child->data.view.getBegin() - old_root_begin;
					child->data.view
						= { new_view.getBegin() + offset_from_start, child->data.view.size() };
					self(child);
				}
			};

			block->data.view = new_view;
			rebase_children_recursively(block);
		}

	private:
		/**
		 * @brief Executes destructors on individual objects that are in the block.
		 * @param block The block to source the data from.
		 */
		void runDataDestructors(Ref<BlockT> block) {
			iterateOverDataAndExecute(block, &GenericMemory::runObjectDestructor);
		}

		/**
		 * @brief Executes copy constructors on individual objects that are in the block.
		 * @param block The block to source the data from.
		 */
		void runDataCopyConstructors(Ref<BlockT> block) {
			iterateOverDataAndExecute(block, &GenericMemory::runObjectCopyConstructor);
		}

		/**
		 * @brief Executes copy constructors on a range of objects, that lay next to each other.
		 */
		void runDataCopyConstructors(base::TypedModRawView<EntryT> data, TypeCRef type) {
			iterateOverDataAndExecute(data, type, &GenericMemory::runObjectCopyConstructor);
		}

		/**
		 * @brief Iterates over each object in the block and calls the callback on it.
		 * @note The callback should not change the layout of the objects in the block.
		 * @param callback A function that will be called on each object.
		 */
		void iterateOverDataAndExecute(
			Ref<BlockT> block,
			void (GenericMemory::*callback)(base::TypedModRawView<EntryT> data, TypeCRef type)
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
			void (GenericMemory::*callback)(base::TypedModRawView<EntryT> data, TypeCRef type)
		) {
			// The algorithm used to iterate over data works as follows:
			// Invariants:
			// * `data` is a range of one or more objects of `type` laying next to each other.
			//   A dynamic table is the exception: its type carries no size, so such a range is
			//   always a single table spanning the whole `data`.
			//
			// Algorithm, for every object of the range:
			// 1. Run the `callback` on the object.
			//    (This is not possible on DynamicTable, see above.)
			//    This is the crucial step, the only place where `callback` is used.
			//    The `callback` therefore runs top-down, not bottom-up in terms of
			//    type-composition.
			// 2. If the type is an aggregate, then step into each member and recurse. Recursion
			//    depth is bounded by the type's nesting depth - ranges are walked by the loop, not
			//    by recursing on the tail.

			const bool  is_dynamic_table = type->getKind() == Type::Kind::DynamicTable;
			const usize object_size      = is_dynamic_table ? data.size() : type->getSize().asInt();

			// Nothing to walk in an empty range, and a zero-sized object would never advance the
			// loop. Such a type cannot hold a pointer either, so there is nothing for a callback
			// to do.
			if (object_size == 0) return;

			CORE_ASSERT(
				data.size() % object_size == 0, "The data must hold a whole number of objects"
			);

			for (usize object_begin = 0; object_begin + object_size <= data.size();
			     object_begin += object_size) {
				const base::TypedModRawView<EntryT> object{ data.getBegin() + object_begin,
					                                        object_size };

				(this->*callback)(object, type);

				switch (type->getKind()) {
				case Type::Kind::Variant:
					// @note We are not touching the variant here,
					// because variant's nested blocks perform needed `callback`s
					// on their own, e.g. in `freeBlockData`.
				case Type::Kind::Primitive:
				case Type::Kind::Function:
				case Type::Kind::Opaque:
				case Type::Kind::CPointer:
				case Type::Kind::Pointer:
					break;
				case Type::Kind::DynamicTable:
				case Type::Kind::FixedSizeTable:
					// The elements lay next to each other, so a single call walks all of them.
					// Note that the range is `object`, not `data`: a fixed size table only owns
					// its own elements, even when several such tables are next to each other.
					iterateOverDataAndExecute(object, type->getInnerType().value(), callback);
					break;
				case Type::Kind::Data:
					// Iterate over data's fields
					for (const auto& fields = **type->getFields(); auto [offset, tp]: fields)
						iterateOverDataAndExecute(
							base::TypedModRawView<EntryT>{ object.getBegin() + offset.asInt(),
						                                   tp->getSize().asInt() },
							tp,
							callback
						);
					break;
				default:
					CORE_PANIC("Handling default");
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
		void runObjectDestructor(base::TypedModRawView<EntryT> data, TypeCRef type)
			requires std::is_same_v<EntryT, byte> {
			switch (type->getKind()) {
			case Type::Kind::Pointer: {
				const auto ptr = safeReadPointerBytes<Pointer>(data.getBegin());
				destroyBlockReference(ptr);
				break;
			}
			case Type::Kind::Primitive:
			case Type::Kind::Function:
			case Type::Kind::Opaque:
			case Type::Kind::CPointer:
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

		/**
		 * @brief Based on data's type, performs copy-constructor of the data.
		 * E.g. in case of a non-null pointer, increases pointed block's reference count.
		 * @note `data` has to represent a single object, not multiple objects - e.g. it can't be a
		 * range of objects from a table, but it can be a single object from a table, or from
		 * somewhere else.
		 */
		void runObjectCopyConstructor(base::TypedModRawView<EntryT> data, TypeCRef type)
			requires std::is_same_v<EntryT, byte> {
			switch (type->getKind()) {
			case Type::Kind::Pointer: {
				const auto ptr = safeReadPointerBytes<Pointer>(data.getBegin());
				if_opt_some(ptr.block.toOpt(), block) increaseBlockRefcount(block);
				break;
			}
			case Type::Kind::Primitive:
			case Type::Kind::Function:
			case Type::Kind::Opaque:
			case Type::Kind::CPointer:
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

	public:
		/**
		 * @brief Executes destructors on a range of objects, that lay next to each other.
		 */
		void runDataDestructors(base::TypedModRawView<EntryT> data, TypeCRef type) {
			iterateOverDataAndExecute(data, type, &GenericMemory::runObjectDestructor);
		}

		GenericMemory() = default;

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
		// Defined in memory.cpp so the header does not need <iostream> for std::cerr.
		bool validateMemoryState() const;

		/**
		 * @brief Frees all the global data
		 */
		void deinitGlobals();

		/**
		 * @brief Free left-over block data, so that VM does not leak memory :)
		 */
		void freeAllocatedBlockData();

		struct GlobalBlocksConfig {
			std::vector<usize>    global_data_offsets;
			std::vector<usize>    global_blocks_idxs;
			std::vector<TypeCRef> global_types;
			Bytes                 total_global_data_size{};
			usize                 global_count{};
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

		auto initializeFrameStack() -> Ref<GenericThreadStack<EntryT>> {
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
		 * @brief Like `allocateDummy`, but leaves the pointed data untouched.
		 *
		 * Used when a block is created for a local variable that was already initialized
		 * without one - zeroing it would destroy the value it holds.
		 */
		auto adoptDummy(TypeCRef type, Ref<EntryT> data_pointer) -> Ref<BlockT> {
			return adoptBlock(dummy_allocator.allocate(type, data_pointer));
		}

		/**
		 * @brief Creates the block of a local variable that was initialized without one, out of
		 * what its slot recorded. Both the executor and the debug adapter go through here.
		 */
		auto createLocalSlotBlock(Frame& frame, u64 slot_index)
			-> Ref<BlockT> requires std::is_same_v<EntryT, byte> {
			const LocalSlot& slot = frame.local_slot_stack_base[slot_index];

			auto block = adoptDummy(slot.type, slot.data);
			// So that nobody can delete our block.
			increaseBlockRefcount(block);

			frame.local_block_ref_stack_base[slot_index] = block.get();
			return block;
		}

		/**
		 * @brief Dynamically reallocates block data.
		 * @note Assumes that type is a dynamic table type and reallocates it to
		   a table of size n with elements of type equal to type's inner type.
		 */
		auto dynTableReallocateBlockDataN(Ref<BlockT> block, u64 n) -> void {
			TypeCRef tbl_type   = block->data.element_type;
			TypeCRef inner_type = tbl_type->getInnerType().value();

			BlockData<EntryT> new_block_data
				= heap_allocator.dynTableAllocateN(tbl_type, inner_type, n);

			changeBlockData(block, new_block_data);
		}

		/**
		 * @brief Frees block's data, but not the block structure itself.
		 * For the block to be freed, use deleteBlock.
		 */
		void freeBlockData(Ref<BlockT> block) {
			for (const auto child: block->children_blocks | std::views::values)
				freeBlockData(child);

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
		 * @note `parent_block` is non-null by construction of `Ref`; callers resolving it from a
		 * `Pointer` get a `VMNullPointerAccessException` from `Pointer::getBlock()` on null.
		 */
		static MRef<BlockT> getNestedViewBlock(Ref<BlockT> parent_block, u64 offset, TypeCRef type) {
			if_opt_some(parent_block->children_blocks.atMaybe(offset), nested) {
				if ((*nested)->data.element_type == type) return *nested;
			}
			return nullptr;
		}

		/**
		 * @brief Creates new block at position.
		 * @note `parent_block` is non-null by construction of `Ref`; callers resolving it from a
		 * `Pointer` get a `VMNullPointerAccessException` from `Pointer::getBlock()` on null.
		 */
		void setNestedViewBlock(Ref<BlockT> parent_block, u64 offset, TypeCRef type) {
			auto& children = parent_block->children_blocks;
			if_opt_some(children.atMaybe(offset), nested) {
				freeBlockData(*nested);
				children.erase(offset);
			}

			auto block_data         = parent_block->data;
			block_data.element_type = type;
			u64 entry_count         = type->getSize().asInt();
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
			requires std::is_same_v<EntryT, byte> {
			increaseBlockRefcount(block);
			return { block, offset };
		}

		[[nodiscard]]
		static constexpr
			__attribute__((always_inline)) auto getPointerData(Pointer pointer, u64 entry_count)
				-> base::TypedModRawView<byte> requires std::is_same_v<EntryT, byte> {
			if (pointer.block == nullptr) throw exceptions::VMNullPointerAccessException();
			if (pointer.block->deallocated) throw exceptions::VMUseAfterFreeException();
			if (pointer.offset + entry_count > pointer.block->data.view.size())
				throw exceptions::VMOutOfBlockBoundsException();
			return { pointer.block->data.view.getBegin() + pointer.offset, entry_count };
		}

		/** @brief Returns the block data from `pointer.offset` to the end of the block. */
		[[nodiscard]]
		static constexpr
			__attribute__((always_inline)) auto getRemainingPointerData(Pointer pointer)
				-> base::TypedModRawView<byte> requires std::is_same_v<EntryT, byte> {
			if (pointer.block == nullptr) throw exceptions::VMNullPointerAccessException();
			if (pointer.block->deallocated) throw exceptions::VMUseAfterFreeException();
			if (pointer.offset > pointer.block->data.view.size())
				throw exceptions::VMOutOfBlockBoundsException();
			return { pointer.block->data.view.getBegin() + pointer.offset,
				     pointer.block->data.view.size() - pointer.offset };
		}

		/**
		 * @brief Copies data pointed-to by `src` to `dst`.
		 * @note Assumes that the size of `type` is known at compile time and (implicitly)
		 * that it is the type of the blocks pointed-to by `dst` and `src`.
		 */
		auto copyPointedData(Pointer dst, Pointer src, TypeCRef type) -> void
			requires std::is_same_v<EntryT, byte> {
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
			requires std::is_same_v<EntryT, byte> {
			if_opt_some(pointer.block.toOpt(), block) { decreaseBlockRefcount(block); }
		}

		auto updatePointerAssignment(Pointer dst, Pointer src) -> Pointer
			requires std::is_same_v<EntryT, byte> {
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

	using Memory                   = GenericMemory<byte>;
	using GlobalBufferPointersByte = GlobalBufferPointers<byte>;

	// The member specialization is defined in initialization_from_const.cpp. It must be declared
	// here so it is visible in every TU before the explicit instantiation of
	// GenericMemory<byte> (in memory.cpp) and any implicit instantiation ([temp.expl.spec]).
	template<>
	void Memory::initializeBlockFromConstValue(
		Ref<Block> block, const code::ConstantValue& const_value
	);

	// Suppress implicit instantiation in every TU that uses `Memory`; the members are emitted once
	// by the explicit instantiation definition in memory.cpp. Must come after the member
	// specialization declaration above.
	extern template class GenericMemory<byte>;
}
