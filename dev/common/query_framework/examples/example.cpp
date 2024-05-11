#include "example.hpp"
#include <query_framework/query_impl.hpp>
#include <query_framework/query_entry_point.hpp>

#include <map>
#include <iostream>

/************
 * QUERY 1: *
 ************/


// make this link less bug-prone...:
struct ImplementationOf_Query1: query::QueryImplementation<Query1, uint64_t> {
	struct InfoInQuery1 final: dia::Info {
		explicit InfoInQuery1(const dia::SourcePosition& source_position): Info(source_position) {}

	protected:
		[[nodiscard]]
		printer::PrinterContent toMessageContentBrief() const override {
			return { "Some random log from Query1." };
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Misc;
		}
	};

	struct ErrorInQuery1 final: dia::Error {
		explicit ErrorInQuery1(const dia::SourcePosition& source_position):
			  Error(source_position) {}

	protected:
		[[nodiscard]]
		printer::PrinterContent toMessageContentBrief() const override {
			return { "An example error in Query1." };
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Misc;
		}
	};

	inline static std::map<QKey, query::CacheEntry<QResult>> cache{};

	// static auto provide(Context& context, QKey key) -> PResult;

	static auto provide(Context& context, QKey key) -> PResult {
		// Log something, with example file path.
		context.log(base::make_unique<InfoInQuery1>(dia::SourcePosition::fakePosition()));
		context.log(base::make_unique<ErrorInQuery1>(dia::SourcePosition::fakePosition()));
		return context.callExt<SquareValue>(key);
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_Query1, "Query 1")

/************
 * QUERY 2: *
 ************/

struct ImplementationOf_Query2: query::QueryImplementation<Query2, uint64_t> {
	inline static std::map<QKey, query::CacheEntry<QResult>> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return key + context.query<Query1>(key + 1);
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_Query2, "Query 2")

/*****************
 * Cyclic Query: *
 *****************/

struct ImplementationOf_CyclicQuery: query::QueryImplementation<CyclicQuery, uint64_t> {
	inline static std::map<QKey, query::CacheEntry<QResult>> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return key + context.query<CyclicQuery>((key + 1) % 5);
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_CyclicQuery, "Cyclic query")

// implement extension:
uint64_t SquareValue(query::Context&, uint64_t v) { return v * v; }

int main() {
	std::cout << query::queryEntryPoint<Query2>(2) << "\n";
	query::debugPrintDependencyGraph();
	std::cout << "\n";

	query::detail::ContextType::logger.dumpLog(true);

	std::cout << query::queryEntryPoint<CyclicQuery>(0) << "\n";

	return 0;
}
