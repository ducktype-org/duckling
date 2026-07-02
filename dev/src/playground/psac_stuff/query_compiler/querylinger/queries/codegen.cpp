#include "codegen.hpp"

#include "inputs.hpp"

#include <psackling/pipeline.hpp>
#include <querylinger/session_state.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <atomic>

namespace ql {
	namespace {
		std::atomic<usize> recompiled_functions{ 0 };

		/// Registers a ConstInput dependency for every compile-time constant the
		/// lowering reads, giving each emission query per-constant granularity.
		class ConstUseRecorder final: public pg::SymbolUseObserver {
		public:
			ConstUseRecorder(query::Context& query_ctx, u64 session_epoch):
				  query_ctx(query_ctx),
				  session_epoch(session_epoch) {}

			void onConstantUse(const pg::sym::Symbol& symbol) override {
				query_ctx.query<ConstInput>(
					{ .session_epoch = session_epoch,
				      .name_hash     = pg::constNameHash(symbol),
				      .value_hash    = pg::constValueHash(activeSession().program.ctx, symbol) }
				);
			}

		private:
			query::Context& query_ctx;
			u64             session_epoch;
		};
	}

	struct IMPLEMENT_QUERY(QueryFunctionObject, ObjectResult) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			SessionState& session = activeSession();
			const usize   index   = session.index_by_link_name[key.link_name];

			// Register the side-input dependencies first, so invalidation reaches this
			// node even when the compilation below fails.
			ctx.query<FnBodyInput>({ .session_epoch = key.session_epoch,
			                         .link_hash     = key.link_hash,
			                         .body_hash     = session.program.fn_hashes[index] });
			ctx.query<GlobalsInput>({ .session_epoch = key.session_epoch,
			                          .globals_hash  = session.program.globals_hash_no_consts });

			recompiled_functions.fetch_add(1, std::memory_order_relaxed);
			ConstUseRecorder recorder(ctx, key.session_epoch);
			try {
				pg::compileOneFunction(
					session.program.ctx,
					session.program.items[index],
					session.object_paths[index],
					&recorder
				);
			} catch (const std::exception& e) { return ObjectResult{ .error = e.what() }; }
			return ObjectResult{};
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryFunctionObject);

	struct IMPLEMENT_QUERY(QueryGlobalsObject, ObjectResult) {
		static auto provide(query::Context& ctx, QKey key) -> PResult {
			SessionState& session = activeSession();

			ctx.query<GlobalsInput>({ .session_epoch = key.session_epoch,
			                          .globals_hash  = session.program.globals_hash_no_consts });

			ConstUseRecorder recorder(ctx, key.session_epoch);
			try {
				pg::emitGlobalsObject(
					session.program.ctx,
					session.program.global_inits,
					session.globals_object_path,
					&recorder
				);
			} catch (const std::exception& e) { return ObjectResult{ .error = e.what() }; }
			return ObjectResult{};
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryGlobalsObject);

	usize functionRecompileCount() { return recompiled_functions.load(std::memory_order_relaxed); }

	void resetFunctionRecompileCount() { recompiled_functions.store(0, std::memory_order_relaxed); }
}
