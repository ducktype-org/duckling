// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "query_framework_decl.hpp"       // query declaration

#include <base/collections/maps.hpp>      // base::Map
#include <base/collections/optional.hpp>  // base::Optional
#include <base/str/str_utils.hpp>         // base::strConcat

#include <diagnostic/message.hpp>
#include <query_framework/standard_query/query_impl.hpp>

/**
 * PResult type for MyQuery
 */
struct PResult final {
	u64 v;
};

struct IMPLEMENT_QUERY(MyQuery, PResult) {
	/**
	 * @brief Some dummy error message for demonstration purposes.
	 *
	 * Search in editor for the `error/misc/example.yaml` to see how the message is defined in the
	 * `yaml` template file.
	 */
	class ExampleError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return {
				.template_type = "message", .type = "error", .family = "misc", .name = "example"
			};
		}

	public:
		ExampleError(dia::SourcePosition source_pos, std::string argument):
			  MessageWithCodeFragmentAndCause(source_pos) {
			addArgument<dia::TextArgument>("argument", std::move(argument));
		}
	};

	/**
	 * Lets define some cache:
	 */
	inline static base::Map<KHash, query::CacheEntry<QResult>> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		// lets call Query2:
		[[maybe_unused]] auto result = context.query<Query2>({ 123 });

		// Normally we would do it because we need
		// it in some computation.
		// Here we will just log it:
		std::string str_to_log = base::strConcat("Result of query 2 : ", result);
		// Dummy source position:
		dia::SourcePosition source_position = dia::SourcePosition::fakePosition();
		context.logInt(makeBox<ExampleError>(source_position, str_to_log));

		// some trivial implementation:
		return PResult{ key.v };
	}

	static auto load(KHash key_hash) -> LoadResult {
		if (cache.contains(key_hash))
			return cache.at(key_hash);
		else
			return {};
	}

	static auto store(KHash key_hash, PResult q_res, query::ACD acd) -> QResult {
		QResult res = { q_res.v };
		cache.put(key_hash, { .data = res, .acd = acd });
		return res;
	}

	static auto erase(KHash key_hash) -> bool { return cache.erase(key_hash) > 0; }
};

QUERY_IMPLEMENTATION_BOILERPLATE(MyQuery);

/**
 * Lets also implement Query2 as a very simple query without any caching:
 */

struct IMPLEMENT_QUERY(Query2, std::string) {
	static auto provide([[maybe_unused]] Context& context, QKey key) -> PResult {
		return std::to_string(key.v);
	}

	static auto load([[maybe_unused]] KHash key_hash) -> LoadResult { return {}; }

	static auto store([[maybe_unused]] KHash key_hash, PResult p_res, [[maybe_unused]] query::ACD acd)
		-> QResult {
		// Here explicit conversion to QResult in not needed, but is left as an example:
		return QResult{ std::move(p_res) };
	}

	static auto erase([[maybe_unused]] KHash key_hash) -> bool { return false; }
};

QUERY_IMPLEMENTATION_BOILERPLATE(Query2);
