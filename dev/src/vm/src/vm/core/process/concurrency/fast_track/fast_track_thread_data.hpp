#pragma once

#include "epoch.hpp"
#include "fast_track_thread_stack.hpp"
#include "shadow_entry.hpp"
#include "shadow_memory.hpp"
#include "vc.hpp"

#include <base/pointers/ref.hpp>

#include <vm/api/data/thread_id.hpp>

#include <algorithm>

namespace vm {
	/**
	 * @brief Raw pointers into the shadow stacks of a Fast Track-enabled thread.
	 *
	 * Mirrors `RuntimeData` for the shadow side: initialised from a `FastTrackThreadStack` and
	 * updated when the global shadow buffer is reallocated.
	 */
	struct FastTrackRuntimeData final {
		ShadowEntry* shadow_data_stack_base     = nullptr;
		ShadowEntry* shadow_data_stack_end      = nullptr;
		ShadowFrame* shadow_frame_stack_base    = nullptr;
		ShadowFrame* shadow_frame_stack_end     = nullptr;
		ShadowFrame* shadow_frame_stack_current = nullptr;

		ShadowEntry*  global_shadow_data_buffer_base  = nullptr;
		ShadowBlock** global_shadow_block_buffer_base = nullptr;

		FastTrackRuntimeData() = default;

		FastTrackRuntimeData(
			Ref<FastTrackThreadStack> stack, ShadowGlobalBufferPointers global_pointers
		):
			  shadow_data_stack_base(stack->getShadowDataStack()->data()),
			  shadow_data_stack_end(
				  stack->getShadowDataStack()->data() + stack->getShadowDataStack()->size()
			  ),
			  shadow_frame_stack_base(stack->getShadowFrameStack()->data()),
			  shadow_frame_stack_end(
				  stack->getShadowFrameStack()->data() + stack->getShadowFrameStack()->size()
			  ),
			  shadow_frame_stack_current(stack->getShadowFrameStack()->data()) {
			updateGlobalPointers(global_pointers);
		}

		void updateGlobalPointers(ShadowGlobalBufferPointers global_pointers) {
			global_shadow_data_buffer_base  = global_pointers.data_buffer_base;
			global_shadow_block_buffer_base = global_pointers.blocks_buffer_base;
		}
	};

	/**
	 * @brief Per-thread Fast Track state: shadow stack pointers and the thread's vector clock.
	 *
	 * Meant to be owned by the Fast Track flavour of the VM thread. All fields are public so that
	 * the opcode functions can access them directly.
	 */
	class FastTrackThreadData final {
	public:
		FastTrackRuntimeData ft_runtime;
		VectorClock          vc;
		api::ThreadID        thread_id = api::ThreadID::bad();

		FastTrackThreadData() = default;

		void init(
			FastTrackThreadStack&      stack,
			api::ThreadID              tid,
			ShadowGlobalBufferPointers global_pointers
		) {
			thread_id  = tid;
			ft_runtime = FastTrackRuntimeData(&stack, global_pointers);
			vc[tid]    = 1;
		}

		void updateGlobalPointers(ShadowGlobalBufferPointers global_pointers) {
			ft_runtime.updateGlobalPointers(global_pointers);
		}

		// ---- Vector clock operations ----

		/**
		 * @brief Lock acquire: C_t := C_t |_| L_m.
		 */
		void onAcquire(const VectorClock& lock_vc) { vc |= lock_vc; }

		/**
		 * @brief Lock release: L_m := C_t; C_t := inc_t(C_t).
		 */
		void onRelease(VectorClock& lock_vc) {
			lock_vc = vc;
			vc.increment(thread_id);
		}

		/**
		 * @brief Thread join: C_t := C_t |_| C_u.
		 */
		void joinVC(const VectorClock& other_vc) { vc |= other_vc; }

		/**
		 * @brief Thread fork, on the child's side: C_u := C_t, with the child's own component
		 * past every clock this thread ID has used so far.
		 *
		 * Thread IDs are pool slots and get reused. This object outlives the thread that had the
		 * slot before, so its own component is the last clock handed out for the ID; a new thread
		 * starting at or below it would have its accesses ordered before accesses that only
		 * happened-after the old thread's, and a real race would go unreported.
		 *
		 * @warning One clock component cannot stand for two threads: starting past the old
		 * thread's clock makes every access of the old thread look ordered before the new one.
		 * That is sound only when the spawner has synchronized with the slot's previous thread
		 * (`parent_vc[my_tid]` is that thread's last clock, e.g. the spawner joined it). Whoever
		 * hands out thread IDs has to guarantee it, or give each spawn a fresh ID.
		 */
		void forkVC(const VectorClock& parent_vc, api::ThreadID my_tid) {
			const Epoch::Clock previous_own = vc[my_tid];
			thread_id                       = my_tid;
			vc                              = parent_vc;
			vc[my_tid]                      = std::max(parent_vc[my_tid], previous_own);
			vc.increment(my_tid);
		}

		[[nodiscard]] const VectorClock& getVC() const { return vc; }

		[[nodiscard]] Epoch getCurrentEpoch() const { return { thread_id, vc[thread_id] }; }

		// ---- Shadow frame / global accessors ----

		[[nodiscard]] ShadowFrame* getShadowFrame() const {
			return ft_runtime.shadow_frame_stack_current;
		}

		[[nodiscard]] ShadowEntry* getGlobalShadowDataBase() const {
			return ft_runtime.global_shadow_data_buffer_base;
		}
	};
}
