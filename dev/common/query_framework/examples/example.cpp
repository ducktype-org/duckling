#include "example.hpp"
#include <diagnostic/diagnostic_converters.hpp>
#include <query_framework/query_impl.hpp>
#include <query_framework/query_entry_point.hpp>

#include <map>
#include <iostream>

/************
 * QUERY 1: *
 ************/


// make this link less bug-prone...:
struct IMPLEMENT_QUERY(Query1, uint64_t) {
	struct InfoInQuery1 final: dia::Info {
		explicit InfoInQuery1(const dia::SourcePosition& source_position): Info(source_position) {}

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Some random log from Query1.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Misc;
		}
	};

	struct ErrorInQuery1 final: dia::Error {
		struct NoteInQuery1 final: dia::Note {
			[[nodiscard]]
			std::string toStringBrief() const override {
				return "A useless note in an example error in Query1.";
			}
		};

		explicit ErrorInQuery1(const dia::SourcePosition& source_position): Error(source_position) {
			addNote(base::make_unique<NoteInQuery1>());
		}

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "An example error in Query1.";
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

QUERY_IMPLEMENTATION_BOILERPLATE(Query1);

/************
 * QUERY 2: *
 ************/

struct IMPLEMENT_QUERY(Query2, uint64_t) {
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

QUERY_IMPLEMENTATION_BOILERPLATE(Query2);

/*****************
 * Cyclic Query: *
 *****************/

struct IMPLEMENT_QUERY(CyclicQuery, uint64_t) {
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

QUERY_IMPLEMENTATION_BOILERPLATE(CyclicQuery);

// implement extension:
uint64_t SquareValue(query::Context&, uint64_t v) { return v * v; }

int main() {
	// The instant logs may be printed in a different order than when they are dumped,
	// because the instant logging is instant, while the dumping is ordered.

	std::cerr << query::entryPoint<Query2>(2) << "\n";
	query::debugPrintDependencyGraph();
	std::cerr << "\n";

	std::cerr << "Here are the logs in user readable form:\n";
	query::detail::ContextType::logger.dumpLog(true);

	std::cerr << "And here are the logs in JSON:\n";
	query::detail::ContextType::logger.dumpLog<dia::DiagnosticToJSONConverter>(true);

	std::cerr << query::entryPoint<CyclicQuery>(0) << "\n";

	return 0;
}
