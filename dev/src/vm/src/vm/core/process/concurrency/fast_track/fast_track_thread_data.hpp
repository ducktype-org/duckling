#pragma once

#include <vm/core/process/concurrency/fast_track/epoch.hpp>
#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/process/concurrency/fast_track/shadow_pointer.hpp>
#include <vm/core/process/concurrency/fast_track/vc.hpp>
#include <vm/core/safe/memory/fast_track_thread_stack.hpp>
#include <vm/core/safe/memory/frame.hpp>

#include <vm/api/data/thread_id.hpp>

namespace vm {

	/**
	 * @brief Raw pointers into the shadow stacks of a FastTrack-enabled thread.
	 *
	 * Mirrors RuntimeData for the shadow side: initialised from a FastTrackThreadStack and
	 * updated when global shadow buffers are reallocated.
	 */
	struct FastTrackRuntimeData {
		ShadowEntry*         shadow_data_stack_base         = nullptr;
		ShadowPointer*       shadow_pointer_stack_base      = nullptr;
		ShadowBlock**        shadow_block_ref_stack_base    = nullptr;
		ShadowBlock**        shadow_block_ref_stack_end     = nullptr;
		ShadowPointerBlock** shadow_pointer_block_ref_stack_base = nullptr;
		ShadowPointerBlock** shadow_pointer_block_ref_stack_end  = nullptr;
		ShadowFrame*         shadow_frame_stack_base        = nullptr;
		ShadowFrame*         shadow_frame_stack_end         = nullptr;
		ShadowFrame*         shadow_frame_stack_current     = nullptr;

		ShadowEntry*   global_shadow_data_buffer_base    = nullptr;
		ShadowPointer* global_shadow_pointer_buffer_base = nullptr;

		FastTrackRuntimeData() = default;

		FastTrackRuntimeData(
			Ref<FastTrackThreadStack> stack,
			ShadowEntry*             global_data,
			ShadowPointer*           global_ptrs
		):
			  shadow_data_stack_base(stack->getShadowDataStack()->data()),
			  shadow_pointer_stack_base(stack->getShadowPointerStack()->data()),
			  shadow_block_ref_stack_base(stack->getShadowBlockRefStack()->data()),
			  shadow_block_ref_stack_end(
				  stack->getShadowBlockRefStack()->data()
				  + stack->getShadowBlockRefStack()->size()
			  ),
			  shadow_pointer_block_ref_stack_base(stack->getShadowPointerBlockRefStack()->data()),
			  shadow_pointer_block_ref_stack_end(
				  stack->getShadowPointerBlockRefStack()->data()
				  + stack->getShadowPointerBlockRefStack()->size()
			  ),
			  shadow_frame_stack_base(stack->getShadowFrameStack()->data()),
			  shadow_frame_stack_end(
				  stack->getShadowFrameStack()->data() + stack->getShadowFrameStack()->size()
			  ),
			  shadow_frame_stack_current(stack->getShadowFrameStack()->data()),
			  global_shadow_data_buffer_base(global_data),
			  global_shadow_pointer_buffer_base(global_ptrs) {}

		void updateGlobalPointers(ShadowEntry* global_data, ShadowPointer* global_ptrs) {
			global_shadow_data_buffer_base    = global_data;
			global_shadow_pointer_buffer_base = global_ptrs;
		}
	};

	/**
	 * @brief Per-thread Fast Track state: shadow stack pointers and vector clock.
	 *
	 * Owned by FastTrackSafeVMThread. All fields are public so that OpFuns (declared
	 * friend in FastTrackSafeVMThread) can access them directly for maximum performance.
	 */
	class FastTrackThreadData {
	public:
		FastTrackRuntimeData ft_runtime;
		VectorClock          vc;
		api::ThreadID        thread_id;

		FastTrackThreadData() = default;

		void init(
			FastTrackThreadStack& stack,
			api::ThreadID         tid,
			ShadowEntry*          global_data,
			ShadowPointer*        global_ptrs
		) {
			thread_id  = tid;
			ft_runtime = FastTrackRuntimeData(&stack, global_data, global_ptrs);
			vc[tid]    = 1;
		}

		void updateGlobalPointers(ShadowEntry* global_data, ShadowPointer* global_ptrs) {
			ft_runtime.updateGlobalPointers(global_data, global_ptrs);
		}

		// ---- Vector clock operations ----

		void onAcquire(const VectorClock& lock_vc) { vc |= lock_vc; }

		void onRelease(VectorClock& lock_vc) {
			lock_vc       = vc;
			vc[thread_id] = vc[thread_id] + 1;
		}

		void joinVC(const VectorClock& other_vc) { vc |= other_vc; }

		void forkVC(const VectorClock& parent_vc, api::ThreadID my_tid) {
			thread_id  = my_tid;
			vc         = parent_vc;
			vc[my_tid] = 1;
		}

		[[nodiscard]] const VectorClock& getVC() const { return vc; }

		[[nodiscard]] Epoch getCurrentEpoch() const {
			return Epoch(thread_id, vc[thread_id]);
		}

		// ---- Shadow frame / global accessors ----

		[[nodiscard]] ShadowFrame* getShadowFrame() const {
			return ft_runtime.shadow_frame_stack_current;
		}

		[[nodiscard]] ShadowEntry* getGlobalShadowDataBase() const {
			return ft_runtime.global_shadow_data_buffer_base;
		}

		[[nodiscard]] ShadowPointer* getGlobalShadowPointerBase() const {
			return ft_runtime.global_shadow_pointer_buffer_base;
		}
	};
}
