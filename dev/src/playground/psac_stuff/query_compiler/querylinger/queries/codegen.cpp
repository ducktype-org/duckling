#include "codegen.hpp"

#include "inputs.hpp"

#include <psackling/pipeline.hpp>
#include <querylinger/session_state.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <atomic>

namespace ql {
	namespace {
		/// Registers a side-input dependency for every global symbol the lowering
		/// uses, giving each emission query per-symbol granularity.
		class SymbolUseRecorder final: public pg::SymbolUseObserver {
		public:
			SymbolUseRecorder(query::Context& query_ctx, const pg::Context& program_ctx):
				  query_ctx(query_ctx),
				  program_ctx(program_ctx) {}

			void onConstantUse(const pg::sym::Symbol& symbol) override {
				query_ctx.query<ConstInput>({ .hash = pg::constInputHash(program_ctx, symbol) });
			}

			void onFunctionUse(const pg::sym::Symbol& symbol) override {
				query_ctx.query<SigInput>({ .hash = pg::functionSigInputHash(program_ctx, symbol) });
			}

			void onGlobalVariableUse(const pg::sym::Symbol& symbol) override {
				query_ctx.query<GlobalVarInput>({ .hash
				                                  = pg::globalVarInputHash(program_ctx, symbol) });
			}

			void onClassTypeUse(const pg::tp::TypeInfo& type_info) override {
				query_ctx.query<ClassInput>({ .hash
				                              = pg::classLayoutInputHash(program_ctx, type_info) });
			}

		private:
			query::Context&    query_ctx;
			const pg::Context& program_ctx;
		};
	}

	struct IMPLEMENT_QUERY(QueryFunctionObject, ObjectResult) {
		static auto provide(query::Context& ctx, const QKey& key) -> PResult {
			const SessionState& session = activeSession();
			const usize         index   = session.index_by_link_name[key.link_name];

			// Register the side-input dependencies first, so invalidation reaches this
			// node even when the compilation below fails.
			ctx.query<FnBodyInput>(
				{ .hash = fnBodyInputHash(key.link_name, session.program.fn_hashes[index]) }
			);
			ctx.query<GlobalsInput>({ .hash = session.program.globals_hash_no_consts });

			session.counters.recompiled_functions.fetch_add(1, std::memory_order_relaxed);
			SymbolUseRecorder recorder(ctx, session.program.ctx);
			try {
				return ObjectResult{ .error        = {},
					                 .object_bytes = pg::compileOneFunctionToBytes(
										 session.program.ctx, session.program.items[index], &recorder
									 ) };
			} catch (const std::exception& e) {
				// The failed lowering never reached its real dependencies (e.g. the
				// missing callee), so retry on any global-surface change instead.
				ctx.query<WorldInput>({ .hash = session.program.globals_hash });
				return ObjectResult{ .error = e.what(), .object_bytes = {} };
			}
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryFunctionObject);

	struct IMPLEMENT_QUERY(QueryGlobalsObject, ObjectResult) {
		static auto provide(query::Context& ctx, QKey /*key*/) -> PResult {
			const SessionState& session = activeSession();

			ctx.query<GlobalsInput>({ .hash = session.program.globals_hash_no_consts });
			ctx.query<GlobalDefsInput>({ .hash = session.program.global_defs_hash });

			session.counters.reemitted_globals_objects.fetch_add(1, std::memory_order_relaxed);
			SymbolUseRecorder recorder(ctx, session.program.ctx);
			try {
				return ObjectResult{ .error        = {},
					                 .object_bytes = pg::emitGlobalsObjectBytes(
										 session.program.ctx, session.program.global_inits, &recorder
									 ) };
			} catch (const std::exception& e) {
				ctx.query<WorldInput>({ .hash = session.program.globals_hash });
				return ObjectResult{ .error = e.what(), .object_bytes = {} };
			}
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryGlobalsObject);
}
