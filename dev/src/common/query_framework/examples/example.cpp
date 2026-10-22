// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "example.hpp"

#include <diagnostic/message.hpp>
#include <init/init.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <iostream>
#include <map>

/************
 * QUERY 1: *
 ************/


// make this link less bug-prone...:
struct IMPLEMENT_QUERY(Query1, uint64_t) {
	/**
	 * @brief Some dummy docs message for demonstration purposes.
	 *
	 * Search in editor for the `docs/example/example.yaml` to see how the message is defined in the
	 * `yaml` template file.
	 */
	class ExampleDocs final: public dia::MessageBase {
		dia::Metadata getMetadata() const final {
			return {
				.template_type = "message", .type = "docs", .family = "example", .name = "example"
			};
		}

	public:
		ExampleDocs(std::string language_name) {
			addArgument<dia::TextArgument>("language_name", std::move(language_name));
			addArgument<dia::TextArgument>("country_name", "Poland");
		}
	};

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

	inline static std::map<KHash, query::CacheEntry<QResult>> cache{};

	static auto provide(Context& context, QKey key) -> PResult {
		// Log something, with example file path.
		context.logInt(makeBox<ExampleDocs>("Duckling"));
		context.logInt(makeBox<ExampleError>(dia::SourcePosition::fakePosition(), "wrong type"));
		return squareValue(context, key.v);
	}

	static auto load(KHash key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(KHash key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { .data = res, .acd = acd } });
		return res;
	}

	static auto erase(KHash key) -> bool { return cache.erase(key); }
};

QUERY_IMPLEMENTATION_BOILERPLATE(Query1);

/************
 * QUERY 2: *
 ************/

struct IMPLEMENT_QUERY(Query2, uint64_t) {
	inline static std::map<KHash, query::CacheEntry<QResult>> cache;

	static auto provide(Context& ctx, QKey key) -> PResult {
		return key.v + ctx.query<Query1>({ key.v + 1 });
	}

	static auto load(KHash key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(KHash key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { .data = res, .acd = acd } });
		return res;
	}

	static auto erase(KHash key) -> bool { return cache.erase(key); }
};

QUERY_IMPLEMENTATION_BOILERPLATE(Query2);

/*****************
 * Cyclic Query: *
 *****************/

struct IMPLEMENT_QUERY(CyclicQuery, uint64_t) {
	inline static std::map<KHash, query::CacheEntry<QResult>> cache;

	static auto provide(Context& ctx, QKey key) -> PResult {
		return key.v + ctx.query<CyclicQuery>({ (key.v + 1) % 5 });
	}

	static auto load(KHash key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(KHash key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { .data = res, .acd = acd } });
		return res;
	}

	static auto erase(KHash key) -> bool { return cache.erase(key); }
};

QUERY_IMPLEMENTATION_BOILERPLATE(CyclicQuery);

// implement extension:
uint64_t squareValue(query::Context&, uint64_t v) { return v * v; }

int main() {
	init::InitObject _;
	// The instant logs may be printed in a different order than when they are dumped,
	// because the instant logging is instant, while the dumping is ordered.

	std::cerr << query::entryPoint<Query2>({ 2 }) << "\n";
	query::internal::ContextAccess::getState()->getGraph().debugPrintForDrawing(std::cerr);
	std::cerr << "\n";

	std::cerr << "Here are the logs in user readable form:\n";
	auto logger = query::Context::dumpToOneLoggerAndClear();
	logger->terminalPrint(std::cerr);

	std::cerr << query::entryPoint<CyclicQuery>({ 0 }) << "\n";

	return 0;
}
