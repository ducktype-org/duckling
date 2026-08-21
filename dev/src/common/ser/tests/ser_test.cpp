/**
 * @file ser_test.cpp
 *
 * The standalone ser repository tests the library with 205 cases across ten files plus
 * 33 compile-failure targets. This is deliberately not that suite: it is a handful of
 * deeply nested structures that exercise every path at once and finish in well under a
 * second, which is what a module test in this repository is for. What each test pins is
 * written above it; the exhaustive per-rule cases stay upstream.
 */

#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include <ser/ser.hpp>
#include <ser/std/all.hpp>
#include <ser/test.hpp>
#include <tester/tester.hpp>

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>


using ByteBuf = std::vector<std::byte>;

/** @brief The whole buffer as the span ser::read wants. */
std::span<const std::byte> view(const ByteBuf& b) { return { b.data(), b.size() }; }

/** @brief The first `n` bytes of it, for the truncation cases. */
std::span<const std::byte> view(const ByteBuf& b, usize n) {
	return { b.data(), n < b.size() ? n : b.size() };
}


// ═══════════════════════════════════════════════════════════════════════════════════
//  Structure one: the data shapes.
//
//  Four levels of aggregate, with every std adapter and both enum kinds somewhere
//  inside. Not one line of serialization code: an aggregate is walked field by field
//  through structured bindings, and the adapters are ser::serializer specializations
//  that <ser/std/all.hpp> brings in.
// ═══════════════════════════════════════════════════════════════════════════════════

enum class Color : u16 { Red = 1, Green = 2, Blue = 65'535 };

enum Shape { ROUND = 0, SQUARE = 3 };  // unscoped, and the compiler picks its underlying type

/**
 * @brief Level 0, and the one type here that has to say how many fields it has.
 *
 * The field-count probe copy-initializes one clause per field, and nothing converts to
 * `char[4]` - brace elision gives that field one clause per ELEMENT instead, so the probe
 * answers 8 for these 5 fields. `ser_members` skips it. Left uncorrected this is a
 * compile error inside the bindings ladder, never wrong bytes.
 */
struct Leaf {
	i32    a      = 0;
	double b      = 0;
	bool   c      = false;
	Color  d      = Color::Red;
	char   tag[4] = {};

	using ser_members = ser::members<5>;

	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index): comparing tag[]
	friend bool operator==(const Leaf&, const Leaf&) = default;
};

/**
 * @brief Level 1: every std adapter, and a duckling strong typedef.
 *
 * No `ser_members` here on purpose - this is the probe doing its job, including over
 * `std::string` and `u8`, which are class types with constructors of their own.
 */
struct Branch {
	std::string                   name;
	u8                            depth = u8(0);
	std::vector<Leaf>             leaves;
	std::optional<Leaf>           maybe;
	std::pair<i32, std::string>   labelled;
	std::tuple<u16, double, bool> triple;
	std::array<u32, 3>            fixed = {};

	friend bool operator==(const Branch&, const Branch&) = default;
};

/** @brief Level 2: the associative containers, over the level below. */
struct Trunk {
	Branch                                  main;
	std::vector<Branch>                     branches;
	std::map<std::string, Leaf>             by_name;
	std::unordered_map<u32, Branch>         by_id;
	std::set<i64>                           ids;
	std::optional<std::vector<std::string>> notes;

	friend bool operator==(const Trunk&, const Trunk&) = default;
};

/** @brief Level 3. */
struct Tree {
	u64                version = 0;
	Shape              shape   = ROUND;
	Trunk              trunk;
	std::vector<Trunk> forest;

	friend bool operator==(const Tree&, const Tree&) = default;
};

namespace ser {

	/**
	 * @brief Level 1 of the hook ladder, on a type nobody can edit.
	 *
	 * `u8` is `STRONG_TYPEDEF_INT(u8, uint8_t)`: a class with a private member and
	 * constructors of its own, so the automatic walk cannot see inside it. A
	 * `ser::serializer<T>` specialization is the answer for exactly that, and this is the
	 * pattern for every other strong typedef in the codebase.
	 */
	template<>
	struct serializer<u8> {
		static errc write(writer auto& ar, const u8& x) { return ar(x.asInt()); }

		static errc read(reader auto& ar, u8& x) {
			uchar raw = 0;
			if (const auto e = ar(raw); e != errc::ok) return e;
			x = u8(raw);
			return errc::ok;
		}
	};

}

/** @brief One value of every shape above, with nothing left empty. */
Tree sampleTree() {
	const Leaf leaf_a{
		.a = -7, .b = 2.5, .c = true, .d = Color::Blue, .tag = { 'l', 'e', 'a', 'f' }
	};
	const Leaf leaf_b{
		.a = 11, .b = -0.125, .c = false, .d = Color::Green, .tag = { 'b', '.', '.', '.' }
	};

	Branch branch;
	branch.name     = "branch-one";
	branch.depth    = u8(3);
	branch.leaves   = { leaf_a, leaf_b, leaf_a };
	branch.maybe    = leaf_b;
	branch.labelled = { -1, "labelled" };
	branch.triple   = { u16(9), 3.5, true };
	branch.fixed    = { 1u, 2u, 3u };

	Branch other = branch;
	other.name   = "branch-two";
	other.maybe  = std::nullopt;  // the empty optional has to survive too
	other.leaves.clear();         // and so does the empty vector

	Trunk trunk;
	trunk.main     = branch;
	trunk.branches = { branch, other };
	trunk.by_name  = { { "a", leaf_a }, { "b", leaf_b } };
	trunk.by_id    = { { 1u, branch }, { 2u, other } };
	trunk.ids      = { -5, 0, 5 };
	trunk.notes    = std::vector<std::string>{ "first", "", "third" };

	Trunk bare;
	bare.notes = std::nullopt;

	return Tree{ .version = 0xDE'AD'BE'EF'CA'FEull,
		         .shape   = SQUARE,
		         .trunk   = trunk,
		         .forest  = { trunk, bare } };
}

// ═══════════════════════════════════════════════════════════════════════════════════
//  Structure two: the dispatch shapes.
//
//  Every hook stamps a byte of its own ahead of its payload. Round-tripping proves
//  nothing on its own - there is always a rung underneath (the automatic walk) that
//  would produce the same values out of the same fields, so a broken detector falls
//  through and leaves the test green. The stamp is what turns that into a failure.
// ═══════════════════════════════════════════════════════════════════════════════════

constexpr uchar TRAIT_STAMP  = 0x11;
constexpr uchar MEMBER_STAMP = 0x22;
constexpr uchar PRIV_STAMP   = 0x33;
constexpr uchar ADL_STAMP    = 0x44;

/** @brief One spelling for both directions: writes the stamp, or reads and checks it. */
template<uchar M>
constexpr ser::errc stamp(auto& ar) {
	uchar m = M;
	if (const auto e = ar(m); e != ser::errc::ok) return e;
	return m == M ? ser::errc::ok : ser::errc::invalid_value;
}

/** @brief The same for the make path, which has no room for a code next to the value. */
template<uchar M>
void expectStamp(ser::reader auto& ar) {
	if (ser::read_field<uchar>(ar) != M) ser::throw_error(ser::errc::invalid_value, ar.position());
}

/** @brief Level 1: ser::serializer<T>, in its symmetric `visit` form. */
struct TraitVisit {
	u32 a = 0;
	u32 b = 0;

	friend bool operator==(const TraitVisit&, const TraitVisit&) = default;
};

namespace ser {

	template<>
	struct serializer<TraitVisit> {
		static constexpr errc visit(auto& ar, auto& self) {
			if (const auto e = stamp<TRAIT_STAMP>(ar); e != errc::ok) return e;
			return ar(self.a, self.b);
		}
	};

}

/** @brief Level 2: hooks declared in the class, as the write/read pair. */
struct MemberPair {
	u32 a = 0;
	u32 b = 0;

	static constexpr ser::errc ser_write(ser::writer auto& ar, const MemberPair& x) {
		if (const auto e = stamp<MEMBER_STAMP>(ar); e != ser::errc::ok) return e;
		return ar(x.a, x.b);
	}

	static constexpr ser::errc ser_read(ser::reader auto& ar, MemberPair& x) {
		if (const auto e = stamp<MEMBER_STAMP>(ar); e != ser::errc::ok) return e;
		return ar(x.a, x.b);
	}

	friend bool operator==(const MemberPair&, const MemberPair&) = default;
};

/**
 * @brief Level 2 again, private and built rather than filled.
 *
 * `const` fields and no default constructor, so there is nothing to fill in after the
 * fact: `ser_make` returns a prvalue that initializes the caller's object where it
 * belongs. Private is the case every hook detector has to get right - written outside
 * `ser::access` they would all report false here and this type would silently take the
 * automatic path.
 */
class PrivMake {
public:
	constexpr PrivMake(u32 x, u32 y) noexcept: a(x), b(y) {}

	[[nodiscard]]
	constexpr u32 first() const noexcept {
		return a;
	}

	[[nodiscard]]
	constexpr u32 second() const noexcept {
		return b;
	}

	friend bool operator==(const PrivMake&, const PrivMake&) = default;

	SER_FRIEND

private:
	const u32 a;
	const u32 b;

	static ser::errc ser_write(ser::writer auto& ar, const PrivMake& x) {
		if (const auto e = stamp<PRIV_STAMP>(ar); e != ser::errc::ok) return e;
		return ar(x.a, x.b);
	}

	static PrivMake ser_make(ser::reader auto& ar) {
		expectStamp<PRIV_STAMP>(ar);
		return PrivMake{ ser::read_field<u32>(ar), ser::read_field<u32>(ar) };
	}
};

/** @brief Level 3: ADL, for a type in someone else's namespace. */
namespace geom {

	struct Point {
		double x = 0;
		double y = 0;

		friend bool operator==(const Point&, const Point&) = default;
	};

	// Both overloads, not one `auto&`: the const one is what the write side needs, and
	// leaving it out is the asymmetry the library refuses - a hook found when reading and
	// missed when writing means the two directions disagree about the format.
	template<class Ar>
	ser::errc ser_visit(Ar& ar, Point& p) {
		if (const auto e = stamp<ADL_STAMP>(ar); e != ser::errc::ok) return e;
		return ar(p.x, p.y);
	}

	template<class Ar>
	ser::errc ser_visit(Ar& ar, const Point& p) {
		if (const auto e = stamp<ADL_STAMP>(ar); e != ser::errc::ok) return e;
		return ar(p.x, p.y);
	}

}

/** @brief SER_DESCRIBE names the fields that go on the wire, and skips the rest. */
struct Described {
	i32         x = 0;
	std::string s;
	i32         skipped = 0;  // not described, so it comes back default-constructed

	// SER_DESCRIBE expands to ,
	// which is the field list the round-trip report names its fields from.
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	SER_DESCRIBE(x, s)

	friend bool operator==(const Described&, const Described&) = default;
};

/** @brief The same two fields with no macro - the walk has to produce the same format. */
struct Twin {
	i32         x = 0;
	std::string s;
};

/** @brief The same two fields renamed - a rename must not change the format. */
struct Renamed {
	i32         y = 0;
	std::string s;

	// SER_DESCRIBE expands to ,
	// which is the field list the round-trip report names its fields from.
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	SER_DESCRIBE(y, s)
};

/**
 * @brief SER_DESCRIBE_MAKE: one field list, and a read side that BUILDS.
 *
 * `SER_DESCRIBE` would not do here. It generates `ser_visit`, which reads by writing
 * through a reference to each field, and `const u64 id` cannot be written through.
 */
class Built {
public:
	Built(u64 an_id, std::string a_name): id(an_id), name(std::move(a_name)) {}

	[[nodiscard]]
	u64 identifier() const noexcept {
		return id;
	}

	friend bool operator==(const Built&, const Built&) = default;

	SER_FRIEND

private:
	const u64   id;
	std::string name;

	// SER_DESCRIBE expands to ,
	// which is the field list the round-trip report names its fields from.
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	SER_DESCRIBE_MAKE(Built, id, name)
};

/**
 * @brief All of them in one object.
 *
 * Not default-constructible, because `PrivMake` is not - so the enclosing walk cannot
 * fill this in either and has to build it out of braced initialization, every field a
 * prvalue at its final address. The static_assert below is what states that: it is the
 * difference between this test covering the build path and covering the fill path twice.
 */
struct Hooked {
	TraitVisit  trait;
	MemberPair  member;
	PrivMake    priv;
	geom::Point adl;
	Described   described;
	Built       built;

	friend bool operator==(const Hooked&, const Hooked&) = default;
};

static_assert(!std::is_default_constructible_v<Hooked>, "Hooked must be BUILT on read, not filled");
static_assert(ser::can_enumerate_members_v<Branch>, "the field probe has to count Branch on its own");

// A described aggregate and the same aggregate walked automatically are the same format,
// so adding SER_DESCRIBE to a type invalidates no stream that was already written.
static_assert(ser::schema_hash<Described>() == ser::schema_hash<Twin>());

/** @brief The nested array the depth guard is measured against. */
template<usize Depth>
struct Nest {
	using Type = std::array<typename Nest<Depth - 1>::Type, 1>;
};

template<>
struct Nest<0> {
	using Type = u32;
};

template<usize Depth>
using NestT = typename Nest<Depth>::Type;

class SerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(nestedRoundtrip);
		TESTER_ADD_TEST(hookLadder);
		TESTER_ADD_TEST(envelope);
		TESTER_ADD_TEST(errorPaths);
		TESTER_ADD_TEST(mutatedStreams);
		TESTER_ADD_TEST(formatContract);
	}

	~SerTest() override = default;

private:
	/**
	 * @brief Every data shape at once, plus the two properties of ser::write worth
	 * pinning: the buffer holds exactly the bytes produced, and a second write appends
	 * after the first instead of overwriting it.
	 */
	void nestedRoundtrip() {
		const Tree tree = sampleTree();

		ByteBuf buf;
		ASSERT_TRUE(ser::write(buf, tree).has_value());
		assertTrue(!buf.empty(), "an object of this size cannot serialize to nothing");

		const auto back = ser::read<Tree>(view(buf));
		ASSERT_TRUE(back.has_value());
		assertTrue(back->value == tree, "the tree did not survive the round trip");

		// Appending, not overwriting: the second message lands after the first, and one
		// archive reads both back in order.
		const usize first = buf.size();
		ASSERT_TRUE(ser::write(buf, tree).has_value());
		ASSERT_EQUAL(2 * first, buf.size());

		ser::in ar{ view(buf) };
		Tree    one;
		Tree    two;
		ASSERT_EQUAL(ser::errc::ok, ar(one, two));
		assertTrue(one == tree && two == tree, "two appended messages did not read back");
		ASSERT_EQUAL(buf.size(), ar.position());

		// The library's own round-trip check, which names the field that came back wrong
		// instead of only reporting that something did.
		ASSERT_TRUE(SER_TEST_ROUNDTRIP(tree));
	}

	/**
	 * @brief Every rung of the dispatch ladder, and which one ran.
	 *
	 * The stamp byte and the wire size together: the byte says which rung handled the
	 * type, the size says that rung wrote nothing else.
	 */
	void hookLadder() {
		expectRung<TRAIT_STAMP>(
			TraitVisit{ .a = 1u, .b = 2u }, 1 + 4 + 4, "ser::serializer<T>::visit"
		);
		expectRung<MEMBER_STAMP>(
			MemberPair{ .a = 3u, .b = 4u }, 1 + 4 + 4, "in-class ser_write/ser_read"
		);
		expectRung<PRIV_STAMP>(PrivMake{ 5u, 6u }, 1 + 4 + 4, "private ser_write/ser_make");
		expectRung<ADL_STAMP>(geom::Point{ .x = 1.5, .y = -2.5 }, 1 + 8 + 8, "ADL ser_visit");

		// SER_DESCRIBE has no stamp of its own - it IS the field list - so what it has to
		// prove is the other half: the described fields survive and the skipped one does
		// not come back.
		ByteBuf described;
		ASSERT_TRUE(ser::write(described, Described{ .x = 9, .s = "nine", .skipped = 7 }).has_value()
		);
		const auto described_back = ser::read<Described>(view(described));
		ASSERT_TRUE(described_back.has_value());
		ASSERT_EQUAL(9, described_back->value.x);
		ASSERT_EQUAL(std::string{ "nine" }, described_back->value.s);
		ASSERT_EQUAL(0, described_back->value.skipped);

		// And the whole ladder underneath one automatic walk. Hooked cannot be default
		// constructed, so this is the build path: each field is a prvalue produced by its
		// own rung and placed straight into the object.
		const Hooked hooked{
			.trait     = { .a = 10u, .b = 11u },
			.member    = { .a = 12u, .b = 13u },
			.priv      = PrivMake{ 14u, 15u },
			.adl       = { .x = 0.5, .y = 0.25 },
			.described = { .x = 16, .s = "sixteen", .skipped = 0 },
			.built     = Built{ 17u, "seventeen" },
		};

		ByteBuf composite;
		ASSERT_TRUE(ser::write(composite, hooked).has_value());
		const auto hooked_back = ser::read<Hooked>(view(composite));
		ASSERT_TRUE(hooked_back.has_value());
		assertTrue(hooked_back->value == hooked, "the composite did not survive the round trip");
	}

	/**
	 * @brief The 32-byte envelope, and the order in which it says no.
	 *
	 * The order IS the diagnosis. A stream from the other byte order is not a stream with
	 * a different schema - every scalar in it is reversed - and reporting a schema
	 * mismatch for it would send the reader looking at their struct definitions.
	 */
	void envelope() {
		constexpr u32 MAGIC = 0xD0'CC'00'01u;

		const Tree tree = sampleTree();

		ByteBuf plain;
		ByteBuf framed;
		ASSERT_TRUE(ser::write(plain, tree).has_value());
		ASSERT_TRUE(ser::write(framed, tree, { .header = true, .user_magic = MAGIC }).has_value());
		ASSERT_EQUAL(plain.size() + ser::stream_header::wire_size, framed.size());

		const auto peeked = ser::peek_header(view(framed), MAGIC);
		ASSERT_TRUE(peeked.has_value());
		ASSERT_EQUAL(framed.size(), usize(peeked->total_size()));
		ASSERT_EQUAL(MAGIC, peeked->user_magic());

		constexpr u64 TREE_SCHEMA = ser::schema_hash<Tree>();
		ASSERT_EQUAL(TREE_SCHEMA, peeked->schema_hash);

		const ser::options opts{ .header = true, .user_magic = MAGIC };
		ASSERT_TRUE(ser::read<Tree>(view(framed), opts).has_value());

		// 1. the envelope has to be there at all
		ASSERT_EQUAL(ser::errc::truncated, ser::read<Tree>(view(framed, 16), opts).code());

		// 2. then the magic - somebody else's ser stream, and a damaged one
		ASSERT_EQUAL(ser::errc::bad_magic, ser::read<Tree>(view(framed), { .header = true }).code());
		ByteBuf damaged = framed;
		damaged[1]      = std::byte{ 'X' };
		ASSERT_EQUAL(ser::errc::bad_magic, ser::read<Tree>(view(damaged), opts).code());

		// 3. then the platform, which is checked before anything in the stream is believed
		damaged     = framed;
		damaged[28] = std::byte{ 0xFF };  // the low byte of `flags`
		ASSERT_EQUAL(ser::errc::platform_mismatch, ser::read<Tree>(view(damaged), opts).code());

		// 4. and only then the schema, which is the one step that needs the type
		ASSERT_EQUAL(ser::errc::schema_mismatch, ser::read<Leaf>(view(framed), opts).code());
	}

	/**
	 * @brief What a damaged stream is allowed to do, which is fail.
	 *
	 * The prefix sweep is the important one: no truncation of a valid stream may be
	 * accepted, and none of them may read past the end of the buffer either.
	 */
	void errorPaths() {
		const Tree tree = sampleTree();
		ByteBuf    buf;
		ASSERT_TRUE(ser::write(buf, tree).has_value());

		for (usize n = 0; n < buf.size(); ++n) {
			const auto cut = ser::read<Tree>(view(buf, n));
			assertTrue(!cut.has_value(), base::strConcat("a prefix of ", n, " bytes was accepted"));
		}

		// A bool holding anything but 0 or 1 is undefined behaviour, so the object
		// representation never reaches the wire and what comes back is validated.
		ByteBuf flag;
		ASSERT_TRUE(ser::write(flag, true).has_value());
		ASSERT_EQUAL(usize{ 1 }, flag.size());
		flag[0] = std::byte{ 2 };
		ASSERT_EQUAL(ser::errc::invalid_value, ser::read<bool>(view(flag)).code());

		// The same key twice is a damaged stream, not a merge: keeping the first would
		// turn it into a map smaller than the one that was written.
		ByteBuf duplicate;
		(void) ser::write(duplicate, u64{ 2 });
		(void) ser::write(duplicate, u32{ 1 });
		(void) ser::write(duplicate, u32{ 10 });
		(void) ser::write(duplicate, u32{ 1 });
		(void) ser::write(duplicate, u32{ 20 });
		using Table = std::map<u32, u32>;
		ASSERT_EQUAL(ser::errc::invalid_value, ser::read<Table>(view(duplicate)).code());

		// A length prefix is the first thing an attacker reaches for: it arrives before
		// the elements it counts, so it is checked against what the stream can hold and
		// against the element ceiling BEFORE anything is reserved.
		ByteBuf lying;
		(void) ser::write(lying, u64{ 1'000 });
		using Numbers = std::vector<u32>;
		ASSERT_EQUAL(ser::errc::truncated, ser::read<Numbers>(view(lying)).code());
		ASSERT_EQUAL(ser::errc::truncated, ser::read<std::string>(view(lying)).code());

		ByteBuf absurd;
		(void) ser::write(absurd, u64{ ser::config_global::max_container_elements } + 1);
		ASSERT_EQUAL(ser::errc::message_size, ser::read<Numbers>(view(absurd)).code());

		// The depth counter, checked against the configured limit rather than an
		// accidental one. No braces on `deep`: Clang materializes the whole initializer
		// tree and overflows its own frontend stack somewhere past 200 levels.
		static NestT<ser::config_global::max_depth + 8> deep;
		ByteBuf                                         nested;
		ser::out                                        ar{ nested };
		ASSERT_EQUAL(ser::errc::depth_exceeded, ar(deep));
		ASSERT_EQUAL(usize{ 0 }, ar.depth());  // the guard unwound cleanly
	}

	/**
	 * @brief The property that is not about correctness: no input crashes, hangs or
	 * allocates without bound.
	 *
	 * A mutation is not required to be refused - flipping a byte inside a double is a
	 * perfectly valid stream - only to produce an answer rather than a signal. The seed
	 * is fixed, so a failure here is reproducible.
	 */
	void mutatedStreams() {
		const Tree tree = sampleTree();
		ByteBuf    buf;
		ASSERT_TRUE(ser::write(buf, tree).has_value());

		u64        state = 0x9E'37'79'B9'7F'4A'7C'15ull;
		const auto next  = [&state] {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            return state;
		};

		constexpr usize ROUNDS   = 3'000;
		usize           accepted = 0;
		usize           refused  = 0;

		ByteBuf mutated;
		for (usize i = 0; i < ROUNDS; ++i) {
			mutated = buf;
			for (usize flips = next() % 4 + 1; flips > 0; --flips)
				mutated[next() % mutated.size()] = static_cast<std::byte>(next() & 0xFFu);

			if (ser::read<Tree>(view(mutated)).has_value())
				++accepted;
			else
				++refused;
		}

		ASSERT_EQUAL(ROUNDS, accepted + refused);
		assertTrue(refused > 0, "no mutation was refused - the corpus is not being read at all");
	}

	/**
	 * @brief What schema_hash promises, stated as the equalities and inequalities that
	 * make it useful.
	 */
	void formatContract() {
		// The bytes are the same and a stream really does read back from one into the
		// other, so the format is the same.
		constexpr u64 ORDERED   = ser::schema_hash<std::map<u32, Leaf>>();
		constexpr u64 UNORDERED = ser::schema_hash<std::unordered_map<u32, Leaf>>();
		ASSERT_EQUAL(ORDERED, UNORDERED);

		// The element type is part of the format, and no sizeof of anybody's std::vector
		// enters into it.
		constexpr u64 OF_U32   = ser::schema_hash<std::vector<u32>>();
		constexpr u64 OF_FLOAT = ser::schema_hash<std::vector<float>>();
		assertTrue(OF_U32 != OF_FLOAT, "vector<u32> and vector<float> are not the same format");

		// A rename is not a format change, and that is the whole reason field names stay
		// out of the wire hash: the C++23 and C++26 backends must agree on it, and only
		// one of them knows the names. debug_hash is where they go.
		constexpr u64 DESCRIBED_SCHEMA = ser::schema_hash<Described>();
		constexpr u64 RENAMED_SCHEMA   = ser::schema_hash<Renamed>();
		ASSERT_EQUAL(DESCRIBED_SCHEMA, RENAMED_SCHEMA);

		constexpr u64 DESCRIBED_DEBUG = ser::debug_hash<Described>();
		constexpr u64 RENAMED_DEBUG   = ser::debug_hash<Renamed>();
		assertTrue(DESCRIBED_DEBUG != RENAMED_DEBUG, "debug_hash has to see the rename");

		// A lower bound and nothing else, but it has to be a truthful one: it is what
		// stands between a corrupt length prefix and an allocation.
		static_assert(
			ser::min_wire_size_v<Leaf> >= sizeof(i32) + sizeof(double) + 1 + sizeof(u16) + 4
		);
		static_assert(ser::min_wire_size_v<std::vector<Leaf>> >= sizeof(u64));
	}

	// ── helpers ─────────────────────────────────────────────────────────────────────

	/** @brief Writes `sample`, checks which rung produced the bytes, and reads it back. */
	template<uchar M, class T>
	void expectRung(const T& sample, usize wire_size, std::string_view what) {
		ByteBuf buf;
		assertTrue(ser::write(buf, sample).has_value(), base::strConcat("write failed: ", what));
		assertTrue(
			buf.size() == wire_size,
			base::strConcat("wrong wire size for ", what, ": ", buf.size(), " != ", wire_size)
		);
		assertTrue(buf[0] == std::byte{ M }, base::strConcat("a different rung handled ", what));

		const auto back = ser::read<T>(view(buf));
		assertTrue(back.has_value(), base::strConcat("read failed: ", what));
		assertTrue(back->value == sample, base::strConcat("value changed: ", what));
	}
};

TESTER_COMMON_MAIN("/src/common/ser/tests/");
