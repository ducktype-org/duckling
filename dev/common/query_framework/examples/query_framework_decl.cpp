#include "query_framework_decl.hpp"  // query declaration

#include <base/maps.hpp>             // base::Map
#include <base/optional.hpp>         // base::Optional
#include <base/str_utils.hpp>        // base::strConcat

#include <query_framework/query_impl.hpp>

/**
 * PResult type for MyQuery
 */
struct PResult {
	uint64_t v;
};

struct IMPLEMENT_QUERY(MyQuery, PResult) {
	struct InfoInMyQuery final: dia::Info {
		std::string str;

		explicit InfoInMyQuery(const dia::SourcePosition& source_position, std::string str):
			  Info(source_position),
			  str(std::move(str)) {}

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return { "Some random log from Query1." };
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Misc;
		}
	};

	/**
	 * Lets define some cache:
	 */
	inline static base::Map<QKey, query::CacheEntry<QResult>> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		// lets call Query2:
		[[maybe_unused]] auto result = context.query<Query2>(123);

		// Normally we would do it because we need
		// it in some computation.
		// Here we will just log it:
		std::string str_to_log = base::strConcat("Result of query 2 : ", result);
		// Dummy source position:
		dia::SourcePosition source_position = dia::SourcePosition::fakePosition();
		context.log(makeBox<InfoInMyQuery>(source_position, str_to_log));

		// some trivial implementation:
		return PResult{ key.v };
	}

	static auto load(QKey key) -> LoadResult {
		// @FUTURE: use atMaybe when one is added to base::Map
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(QKey key, PResult q_res, query::ACD acd) -> QResult {
		QResult res = { q_res.v };
		cache.put(key, { res, acd });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(MyQuery);

/**
 * Lets also implement Query2 as a very simple query without any caching:
 */

struct IMPLEMENT_QUERY(Query2, std::string) {
	static auto provide([[maybe_unused]] Context& context, QKey key) -> PResult {
		return std::to_string(key);
	}

	static auto load([[maybe_unused]] QKey key) -> LoadResult { return {}; }

	static auto store([[maybe_unused]] QKey key, PResult p_res, [[maybe_unused]] query::ACD acd)
		-> QResult {
		// Here explicit conversion to QResult in not needed, but is left as an example:
		return QResult{ std::move(p_res) };
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(Query2);
