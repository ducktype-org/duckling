/**
 * @file ser_test.cpp
 *
 * The standalone ser repository tests the library with 205 cases across ten files plus
 * 33 compile-failure targets. This is deliberately not that suite: it is a handful of
 * deeply nested structures that exercise every path at once and finish in well under a
 * second, which is what a module test in this repository is for. What each test pins is
 * written above it; the exhaustive per-rule cases stay upstream.
 */

#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include <ser/ser.hpp>
#include <ser/std/all.hpp>
#include <ser/test.hpp>
#include <tester/tester.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>


using ByteBuf = std::vector<std::byte>;

/** @brief The whole buffer as the span ser::read wants. */
std::span<const std::byte> view(const ByteBuf& b) { return { b.data(), b.size() }; }

/** @brief The first `n` bytes of it, for the truncation cases. */
std::span<const std::byte> view(const ByteBuf& b, usize n) {
	return { b.data(), n < b.size() ? n : b.size() };
}


/**
 * @brief ═══════════════════════════════════════════════════════════════════════════════════
 *  Structure one: the data shapes.
 *
 *  Four levels of aggregate, with every std adapter and both enum kinds somewhere
 *  inside. Not one line of serialization code: an aggregate is walked field by field
 *  through structured bindings, and the adapters are ser::Serializer specializations
 *  that <ser/std/all.hpp> brings in.
 * ═══════════════════════════════════════════════════════════════════════════════════
 */

enum class Color : u16 { Red = 1, Green = 2, Blue = 65'535 };

enum Shape { ROUND = 0, SQUARE = 3 };  // unscoped, and the compiler picks its underlying type

/**
 * @brief Zero bytes in the stream, whatever the count - which is what makes a container of them the
 * one place a policy ceiling has to do the bounding.
 */
struct Nothing {
	friend bool operator==(const Nothing&, const Nothing&) = default;
};

/**
 * @brief An unscoped enum with no fixed underlying type has no queryable value range, and
 * static_cast to it is only defined inside that range - so the range is declared here and
 * readEnum refuses anything outside it. An `enum Shape : u16` would need none of this.
 */
template<>
struct ser::EnumRange<Shape> {
	static constexpr Shape MIN = ROUND;
	static constexpr Shape MAX = SQUARE;
};

/**
 * @brief Level 0, and the one type here that has to say how many fields it has.
 *
 * The field-count probe copy-initializes one clause per field, and nothing converts to
 * `char[4]` - brace elision gives that field one clause per ELEMENT instead, so the probe
 * answers 8 for these 5 fields. `ser_members` skips it. Left uncorrected this is a
 * compile error inside the bindings ladder, never wrong bytes.
 */
struct Leaf {
	i32    a = 0;
	double b = 0;
	bool   c = false;
	Color  d = Color::Red;
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	char tag[4] = {};

	using ser_members = ser::Members<5>;

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
	 * @brief Level 1 of the hook ladder, and what it beats.
	 *
	 * `u8` is `STRONG_TYPEDEF_INT(u8, uint8_t)`: a class with a private member, so the
	 * automatic walk cannot see inside it - which is why that macro declares a `serVisit` of
	 * its own, and every strong typedef in the codebase round-trips with no code at all. This
	 * specialization is therefore not what makes `u8` serializable; it is what makes level 1
	 * OUTRANK that in-class hook, and the two produce the same byte either way.
	 */
	template<>
	struct Serializer<u8> {
		static Errc write(Writer auto& ar, const u8& x) { return ar(x.asInt()); }

		static Errc read(Reader auto& ar, u8& x) {
			uchar raw = 0;
			if (const auto e = ar(raw); e != Errc::Ok) return e;
			x = u8(raw);
			return Errc::Ok;
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

/**
 * @brief ═══════════════════════════════════════════════════════════════════════════════════
 *  Structure two: the dispatch shapes.
 *
 *  Every hook stamps a byte of its own ahead of its payload. Round-tripping proves
 *  nothing on its own - there is always a rung underneath (the automatic walk) that
 *  would produce the same values out of the same fields, so a broken detector falls
 *  through and leaves the test green. The stamp is what turns that into a failure.
 * ═══════════════════════════════════════════════════════════════════════════════════
 */

constexpr uchar TRAIT_STAMP  = 0x11;
constexpr uchar MEMBER_STAMP = 0x22;
constexpr uchar PRIV_STAMP   = 0x33;
constexpr uchar ADL_STAMP    = 0x44;

/** @brief One spelling for both directions: writes the stamp, or reads and checks it. */
template<uchar M>
constexpr ser::Errc stamp(auto& ar) {
	uchar m = M;
	if (const auto e = ar(m); e != ser::Errc::Ok) return e;
	return m == M ? ser::Errc::Ok : ser::Errc::InvalidValue;
}

/** @brief The same for the make path, which has no room for a code next to the value. */
template<uchar M>
void expectStamp(ser::Reader auto& ar) {
	if (ser::subMake<uchar>(ar) != M) ser::throwError(ser::Errc::InvalidValue, ar.position());
}

/** @brief Level 1: ser::Serializer<T>, in its symmetric `visit` form. */
struct TraitVisit {
	u32 a = 0;
	u32 b = 0;

	friend bool operator==(const TraitVisit&, const TraitVisit&) = default;
};

namespace ser {

	template<>
	struct Serializer<TraitVisit> {
		static constexpr Errc visit(auto& ar, auto& self) {
			if (const auto e = stamp<TRAIT_STAMP>(ar); e != Errc::Ok) return e;
			return ar(self.a, self.b);
		}
	};

}

/** @brief Level 2: hooks declared in the class, as the write/read pair. */
struct MemberPair {
	u32 a = 0;
	u32 b = 0;

	static constexpr ser::Errc serWrite(ser::Writer auto& ar, const MemberPair& x) {
		if (const auto e = stamp<MEMBER_STAMP>(ar); e != ser::Errc::Ok) return e;
		return ar(x.a, x.b);
	}

	static constexpr ser::Errc serRead(ser::Reader auto& ar, MemberPair& x) {
		if (const auto e = stamp<MEMBER_STAMP>(ar); e != ser::Errc::Ok) return e;
		return ar(x.a, x.b);
	}

	friend bool operator==(const MemberPair&, const MemberPair&) = default;
};

/**
 * @brief Level 2 again, private and built rather than filled.
 *
 * `const` fields and no default constructor, so there is nothing to fill in after the
 * fact: `serMake` returns a prvalue that initializes the caller's object where it
 * belongs. Private is the case every hook detector has to get right - written outside
 * `ser::Access` they would all report false here and this type would silently take the
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

	static ser::Errc serWrite(ser::Writer auto& ar, const PrivMake& x) {
		if (const auto e = stamp<PRIV_STAMP>(ar); e != ser::Errc::Ok) return e;
		return ar(x.a, x.b);
	}

	static PrivMake serMake(ser::Reader auto& ar) {
		expectStamp<PRIV_STAMP>(ar);
		return PrivMake{ ser::subMake<u32>(ar), ser::subMake<u32>(ar) };
	}
};

/** @brief Level 3: ADL, for a type in someone else's namespace. */
namespace geom {

	struct Point {
		double x = 0;
		double y = 0;

		friend bool operator==(const Point&, const Point&) = default;
	};

	/**
	 * Both overloads, not one `auto&`: the const one is what the write side needs, and
	 * leaving it out is the asymmetry the library refuses - a hook found when reading and
	 * missed when writing means the two directions disagree about the format.
	 */
	template<ser::ReaderOrWriter Ar>
	ser::Errc serVisit(Ar& ar, Point& p) {
		if (const auto e = stamp<ADL_STAMP>(ar); e != ser::Errc::Ok) return e;
		return ar(p.x, p.y);
	}

	template<ser::ReaderOrWriter Ar>
	ser::Errc serVisit(Ar& ar, const Point& p) {
		if (const auto e = stamp<ADL_STAMP>(ar); e != ser::Errc::Ok) return e;
		return ar(p.x, p.y);
	}

}

/** @brief SER_DESCRIBE names the fields that go in the stream, and skips the rest. */
struct Described {
	i32         x = 0;
	std::string s;
	i32         skipped = 0;  // not described, so it comes back default-constructed

	/** SER_DESCRIBE expands to the field list the round-trip report names its fields from. */
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

	/** SER_DESCRIBE expands to the field list the round-trip report names its fields from. */
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	SER_DESCRIBE(y, s)
};

/**
 * @brief SER_DESCRIBE_MAKE: one field list, and a read side that BUILDS.
 *
 * `SER_DESCRIBE` would not do here. It generates `serVisit`, which reads by writing
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

	/** SER_DESCRIBE expands to the field list the round-trip report names its fields from. */
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
static_assert(ser::CAN_ENUMERATE_MEMBERS_V<Branch>, "the field probe has to count Branch on its own");

/**
 * A described aggregate and the same aggregate walked automatically are the same format,
 * so adding SER_DESCRIBE to a type invalidates no stream that was already written.
 */
static_assert(ser::schemaHash<Described>() == ser::schemaHash<Twin>());

// ═══════════════════════════════════════════════════════════════════════════════════
//  Structure four: the variant, and the shapes that make it awkward.
//
//  Every question this adapter answers is "which alternative does the TAG name" - never
//  "which alternative does this value convert to". The types below are the cases where
//  those two answers differ, or where the answer costs something.
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * @brief monostate, containers, and an alternative that has to be BUILT.
 *
 * `Built` is the interesting one: a const field, so it is neither default-constructible
 * nor assignable, which makes a Payload unassignable too. That is exactly the shape for
 * which `read` has to exist - emplace replaces an alternative without assigning to it,
 * while the make path could only fill a field by assigning the whole variant.
 */
using Payload = std::variant<std::monostate, i32, std::string, std::vector<Leaf>, Built>;

/** @brief Two alternatives of the SAME type: nothing but the tag can tell them apart. */
using Twins = std::variant<i32, i32>;

/** @brief `Ambiguous{ "text" }` would pick bool if overload resolution had a say here. */
using Ambiguous = std::variant<bool, std::string>;

/** @brief A variant as a field of a type with a serRead, which is the only way an
 *  EXISTING variant is handed to Serializer<Payload>::read rather than being built. */
struct Slot {
	Payload held;

	static constexpr ser::Errc serWrite(ser::Writer auto& ar, const Slot& x) { return ar(x.held); }

	static constexpr ser::Errc serRead(ser::Reader auto& ar, Slot& x) { return ar(x.held); }

	friend bool operator==(const Slot&, const Slot&) = default;
};

/** @brief The same variant as an ordinary field and as a container element. */
struct Tagged {
	u64                  id = 0;
	Payload              payload;
	std::vector<Payload> history;

	friend bool operator==(const Tagged&, const Tagged&) = default;
};

/**
 * @brief Throws while being constructed, which is how a variant becomes valueless with no
 * undefined behaviour involved.
 *
 * The hand-written move constructor is what makes that happen. A throwing constructor is
 * not enough on its own: while the alternative is NOTHROW-move-constructible, libstdc++
 * builds a temporary and moves it in, so the throw leaves the old value untouched and the
 * variant valid. Only a move that may throw forces construction in place, and only then is
 * there a window for the variant to lose its value.
 */
struct Boom {
	i32 a = 0;

	Boom() = default;

	explicit Boom(i32 /*unused*/) { throw std::runtime_error{ "boom" }; }

	Boom(const Boom&) = default;

	Boom(Boom&& other) noexcept(false): a(other.a) {}

	static ser::Errc serWrite(ser::Writer auto& ar, const Boom& x) { return ar(x.a); }

	static ser::Errc serRead(ser::Reader auto& ar, Boom& x) { return ar(x.a); }
};

using Fragile = std::variant<i32, Boom>;

static_assert(
	ser::Serializer<Payload>::FILLABLE,
	"every alternative is reachable, so a Payload field is filled rather than assigned"
);
static_assert(
	!ser::Serializer<Payload>::FILLS_IN_PLACE<Built>,
	"Built has a const field: it is built by dispatchMake and emplaced, not filled"
);
static_assert(
	!std::is_move_assignable_v<Payload>,
	"which is why Serializer<Payload>::read has to exist - `x = make(ar)` would not compile"
);

/** The tag, plus the smallest alternative - and monostate writes nothing at all. */
static_assert(ser::MIN_SERIALIZED_SIZE_V<std::monostate> == 0);
static_assert(ser::MIN_SERIALIZED_SIZE_V<Payload> == sizeof(u64));
static_assert(ser::MIN_SERIALIZED_SIZE_V<Twins> == sizeof(u64) + sizeof(i32));

/**
 * A tag means nothing without the alternative LIST it indexes, so order and count are
 * both part of the format - and a variant is not the tuple of the same types.
 */
static_assert(
	ser::schemaHash<std::variant<i32, double>>() != ser::schemaHash<std::variant<double, i32>>()
);
static_assert(
	ser::schemaHash<std::variant<i32, double>>()
	!= ser::schemaHash<std::variant<i32, double, bool>>()
);
static_assert(
	ser::schemaHash<std::variant<i32, double>>() != ser::schemaHash<std::tuple<i32, double>>()
);

/** A cv-qualified alternative is the same serialized type as the alternative itself. */
static_assert(
	ser::schemaHash<std::variant<const i32, std::string>>()
	== ser::schemaHash<std::variant<i32, std::string>>()
);

// ═══════════════════════════════════════════════════════════════════════════════════
//  Structure five: types that contain themselves, and wrappers over one integer.
//
//  Both are about ser::schemaHash rather than about the bytes. A self-referential type
//  makes the schema walk meet a type it is already inside, which is the one shape whose
//  hash cannot be a plain description of the fields; a strong typedef is a type whose
//  format the walk cannot see at all, so it has to say what it is.
// ═══════════════════════════════════════════════════════════════════════════════════

/** @brief Contains itself through a vector - the shape the "recur" token exists for. */
struct Node final {
	i32               value = 0;
	std::vector<Node> kids;

	friend bool operator==(const Node&, const Node&) = default;
};

/**
 * @brief A hook that reads its fields with ser::subMake, which signals by THROWING.
 *
 * The hook is declared to return a code, so the two conventions meet inside dispatch - the
 * shape MetadataStorage::serRead has, and the shape every serMake-based factory has.
 */
struct Thrower final {
	i32 a = 0;
	i32 b = 0;

	friend bool operator==(const Thrower&, const Thrower&) = default;

	static ser::Errc serWrite(ser::Writer auto& ar, const Thrower& self) {
		return ar(self.a, self.b);
	}

	static ser::Errc serRead(ser::Reader auto& ar, Thrower& self) {
		self.a = ser::subMake<i32>(ar);
		self.b = ser::subMake<i32>(ar);
		return ser::Errc::Ok;
	}
};

/** @brief The same, one level of indirection deeper: recursion through two types. */
struct Branchy;

struct Twig final {
	std::vector<Branchy> branches;

	friend bool operator==(const Twig&, const Twig&) = default;
};

struct Branchy final {
	i32  value = 0;
	Twig twig;

	friend bool operator==(const Branchy&, const Branchy&) = default;
};

/** @brief Recursion through an optional rather than a vector: a different shape. */
struct Chain final {
	i32                               value = 0;
	std::optional<std::vector<Chain>> rest;

	friend bool operator==(const Chain&, const Chain&) = default;
};

/**
 * A self-referential type has a hash AT ALL, which is the whole point: the walk meets
 * itself and mixes a back-reference instead of recursing for ever. This is a compile-time
 * property, so the static_assert is the test.
 */
static_assert(ser::schemaHash<Node>() != 0);
static_assert(ser::schemaHash<Node>() == ser::schemaHash<Node>(), "the hash has to be stable");
static_assert(ser::debugHash<Node>() != 0);
static_assert(ser::schemaHash<Branchy>() != 0);
static_assert(ser::schemaHash<Twig>() != 0);

/**
 * And the back-reference carries WHERE it points, so two different recursion shapes over
 * the same field types are different formats.
 */
static_assert(ser::schemaHash<Node>() != ser::schemaHash<Chain>());
static_assert(ser::schemaHash<Node>() != ser::schemaHash<Branchy>());
static_assert(ser::schemaHash<Twig>() != ser::schemaHash<Branchy>());

/**
 * The envelope of a recursive type is buildable too - StreamHeader::forType is where a
 * broken hash of one used to surface.
 */
static_assert(ser::StreamHeader::forType<Node>().schema_hash == ser::schemaHash<Node>());

/** @brief Two ids over the same integer, and one over an integer of the same width. */
STRONG_TYPEDEF_INT(TestIdA, std::uint32_t);
STRONG_TYPEDEF_INT(TestIdB, std::uint32_t);
STRONG_TYPEDEF_INT(TestIdSigned, std::int32_t);

struct KeyedByA final {
	TestIdA id;
	i32     extra = 0;
};

struct KeyedByB final {
	TestIdB id;
	i32     extra = 0;
};

/**
 * A strong typedef writes exactly its integer, and nothing in the class is walkable - the
 * value is private - so without ser_serialize_as it would hash as sizeof + alignof and every
 * one of these would share a number. What has to hold instead: the WIDTH and the SIGN of
 * the wrapped integer are in the hash, and so is the type's own name.
 */
static_assert(ser::schemaHash<TestIdA>() != ser::schemaHash<TestIdB>());
static_assert(ser::schemaHash<TestIdA>() != ser::schemaHash<TestIdSigned>());
static_assert(ser::schemaHash<TestIdA>() != ser::schemaHash<std::uint32_t>());
static_assert(ser::schemaHash<TestIdA>() != ser::schemaHash<u8>());
static_assert(ser::schemaHash<KeyedByA>() != ser::schemaHash<KeyedByB>());

/**
 * And the same alias makes the length-prefix check truthful: a vector of ids can only claim
 * as many elements as four bytes each will fit in the stream, where a hooked type with no
 * alias would fall back to one byte and let a corrupt prefix through four times as far.
 */
static_assert(ser::MIN_SERIALIZED_SIZE_V<TestIdA> == sizeof(std::uint32_t));
static_assert(ser::MIN_SERIALIZED_SIZE_V<u8> == 1);
static_assert(ser::MIN_SERIALIZED_SIZE_V<KeyedByA> == sizeof(std::uint32_t) + sizeof(i32));

/**
 * @brief An aggregate that cannot be filled in place (the const field) with a C array field,
 * so reading it builds it one clause per element.
 */
struct FrozenWithArray {
	const i32 key;
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	u16 codes[3];

	using ser_members = ser::Members<2>;
};

/**
 * @brief Exactly as many members as the structured-bindings table covers.
 *
 * The table in base/preproc/ladder.hpp is 64 hand-generated rungs, and only the last one
 * proves it was generated whole - a chain that stops short fails to compile at the rung it is
 * missing, and one that mis-numbers a rung binds the wrong count.
 */
struct Wide64 final {
	i32 f0  = 0;
	i32 f1  = 1;
	i32 f2  = 2;
	i32 f3  = 3;
	i32 f4  = 4;
	i32 f5  = 5;
	i32 f6  = 6;
	i32 f7  = 7;
	i32 f8  = 8;
	i32 f9  = 9;
	i32 f10 = 10;
	i32 f11 = 11;
	i32 f12 = 12;
	i32 f13 = 13;
	i32 f14 = 14;
	i32 f15 = 15;
	i32 f16 = 16;
	i32 f17 = 17;
	i32 f18 = 18;
	i32 f19 = 19;
	i32 f20 = 20;
	i32 f21 = 21;
	i32 f22 = 22;
	i32 f23 = 23;
	i32 f24 = 24;
	i32 f25 = 25;
	i32 f26 = 26;
	i32 f27 = 27;
	i32 f28 = 28;
	i32 f29 = 29;
	i32 f30 = 30;
	i32 f31 = 31;
	i32 f32 = 32;
	i32 f33 = 33;
	i32 f34 = 34;
	i32 f35 = 35;
	i32 f36 = 36;
	i32 f37 = 37;
	i32 f38 = 38;
	i32 f39 = 39;
	i32 f40 = 40;
	i32 f41 = 41;
	i32 f42 = 42;
	i32 f43 = 43;
	i32 f44 = 44;
	i32 f45 = 45;
	i32 f46 = 46;
	i32 f47 = 47;
	i32 f48 = 48;
	i32 f49 = 49;
	i32 f50 = 50;
	i32 f51 = 51;
	i32 f52 = 52;
	i32 f53 = 53;
	i32 f54 = 54;
	i32 f55 = 55;
	i32 f56 = 56;
	i32 f57 = 57;
	i32 f58 = 58;
	i32 f59 = 59;
	i32 f60 = 60;
	i32 f61 = 61;
	i32 f62 = 62;
	i32 f63 = 63;
};

class SerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(nestedRoundtrip);
		TESTER_ADD_TEST(hookLadder);
		TESTER_ADD_TEST(variants);
		TESTER_ADD_TEST(envelope);
		TESTER_ADD_TEST(errorPaths);
		TESTER_ADD_TEST(mutatedStreams);
		TESTER_ADD_TEST(formatContract);
		TESTER_ADD_TEST(recursiveTypes);
		TESTER_ADD_TEST(memberCountLimit);
		TESTER_ADD_TEST(constQualifiedRead);
		TESTER_ADD_TEST(arrays);
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

		ser::In ar{ view(buf) };
		Tree    one;
		Tree    two;
		ASSERT_EQUAL(ser::Errc::Ok, ar(one, two));
		assertTrue(one == tree && two == tree, "two appended messages did not read back");
		ASSERT_EQUAL(buf.size(), ar.position());

		// The position lives in the ARCHIVE, so one call per message reads the same bytes as
		// one call with both - and position() says where the next one begins, which is the
		// thing ser::read cannot tell a caller.
		ser::In split{ view(buf) };
		Tree    first_back;
		Tree    second_back;
		ASSERT_EQUAL(ser::Errc::Ok, split(first_back));
		ASSERT_EQUAL(first, split.position());
		ASSERT_EQUAL(ser::Errc::Ok, split(second_back));
		ASSERT_EQUAL(buf.size(), split.position());
		assertTrue(
			first_back == tree && second_back == tree, "one call per message read different bytes"
		);

		// The library's own round-trip check, which names the field that came back wrong
		// instead of only reporting that something did.
		ASSERT_TRUE(SER_TEST_ROUNDTRIP(tree));
	}

	/**
	 * @brief Every rung of the dispatch ladder, and which one ran.
	 *
	 * The stamp byte and the serialized size together: the byte says which rung handled the
	 * type, the size says that rung wrote nothing else.
	 */
	void hookLadder() {
		expectRung<TRAIT_STAMP>(
			TraitVisit{ .a = 1u, .b = 2u }, 1 + 4 + 4, "ser::Serializer<T>::visit"
		);
		expectRung<MEMBER_STAMP>(
			MemberPair{ .a = 3u, .b = 4u }, 1 + 4 + 4, "in-class serWrite/serRead"
		);
		expectRung<PRIV_STAMP>(PrivMake{ 5u, 6u }, 1 + 4 + 4, "private serWrite/serMake");
		expectRung<ADL_STAMP>(geom::Point{ .x = 1.5, .y = -2.5 }, 1 + 8 + 8, "ADL serVisit");

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
	 * @brief The tag decides which alternative is in the stream, and nothing else does.
	 *
	 * Four properties, and each one is a thing that goes wrong when an implementation
	 * takes the obvious shortcut: monostate costs no bytes, the tag beats overload
	 * resolution, an unassignable alternative is still readable IN PLACE, and a tag that
	 * names no alternative is refused before anything is built from it.
	 */
	void variants() {
		// monostate writes nothing: the tag is the entire message.
		ByteBuf empty_alternative;
		ASSERT_TRUE(ser::write(empty_alternative, Payload{}).has_value());
		ASSERT_EQUAL(sizeof(u64), empty_alternative.size());
		const auto mono = ser::read<Payload>(view(empty_alternative));
		ASSERT_TRUE(mono.has_value());
		ASSERT_EQUAL(usize{ 0 }, mono->value.index());

		expectAlternative(Payload{ i32{ -7 } }, 1);
		expectAlternative(Payload{ std::string{ "text" } }, 2);
		expectAlternative(Payload{ std::vector<Leaf>{ Leaf{ .a = 1, .b = 0.5 } } }, 3);
		expectAlternative(Payload{ Built{ 5u, "five" } }, 4);  // built, then emplaced

		// Two alternatives of the same type, and one where a converting constructor would
		// have sent "text" to bool. Only the tag distinguishes either case.
		expectAlternative(Twins{ std::in_place_index<0>, 5 }, 0);
		expectAlternative(Twins{ std::in_place_index<1>, 5 }, 1);
		expectAlternative(Ambiguous{ true }, 0);
		expectAlternative(Ambiguous{ std::string{ "text" } }, 1);

		// Nested, and a const alternative - which is not fillable, so it is emplaced.
		expectAlternative(std::variant<i32, Payload>{ Payload{ std::string{ "inner" } } }, 1);
		expectAlternative(std::variant<const i32, std::string>{ std::in_place_index<0>, 3 }, 0);

		// An EXISTING variant, handed over by Slot::serRead. This is the fill path, and
		// the static_asserts above are why it has to exist: a Payload cannot be assigned.
		const Slot slot{ .held = Payload{ Built{ 9u, "nine" } } };
		ByteBuf    slot_bytes;
		ASSERT_TRUE(ser::write(slot_bytes, slot).has_value());
		const auto slot_back = ser::read<Slot>(view(slot_bytes));
		ASSERT_TRUE(slot_back.has_value());
		assertTrue(slot_back->value == slot, "the filled variant did not come back");

		// A tag no alternative answers to, and a tag with nothing behind it. Both are
		// refused before the alternative is touched.
		ByteBuf bad_tag;
		(void) ser::write(bad_tag, u64{ 99 });
		ASSERT_EQUAL(ser::Errc::InvalidValue, ser::codeOf(ser::read<Payload>(view(bad_tag))));

		ByteBuf tag_only;
		(void) ser::write(tag_only, u64{ 2 });  // the std::string alternative, and no string
		ASSERT_EQUAL(ser::Errc::Truncated, ser::codeOf(ser::read<Payload>(view(tag_only))));

		// A valueless variant is refused rather than written: variant_npos names no
		// alternative, so no tag could describe it and nothing may reach the buffer.
		Fragile fragile;
		try {
			fragile.emplace<1>(i32{ 0 });
		} catch (const std::runtime_error&) {}
		static_assert(!std::is_nothrow_move_constructible_v<Boom>, "or it never goes valueless");
		assertTrue(fragile.valueless_by_exception(), "the fixture did not become valueless");
		ByteBuf refused;
		ASSERT_EQUAL(ser::Errc::InvalidValue, ser::codeOf(ser::write(refused, fragile)));
		assertTrue(refused.empty(), "a refused write must not leave bytes behind");

		// And in the shapes it will actually appear in: a field and a container element.
		const Tagged tagged{
			.id      = 7,
			.payload = Payload{ std::string{ "top" } },
			.history = { Payload{}, Payload{ i32{ 1 } }, Payload{ Built{ 2u, "two" } } },
		};

		ByteBuf composite;
		ASSERT_TRUE(ser::write(composite, tagged).has_value());
		const auto tagged_back = ser::read<Tagged>(view(composite));
		ASSERT_TRUE(tagged_back.has_value());
		assertTrue(tagged_back->value == tagged, "the variant fields did not survive");
		ASSERT_TRUE(SER_TEST_ROUNDTRIP(tagged));

		// No truncation of that stream may be accepted: a tag whose alternative is missing
		// is the same class of damage as a length prefix with no elements behind it.
		for (usize n = 0; n < composite.size(); ++n) {
			const auto cut = ser::read<Tagged>(view(composite, n));
			assertTrue(!cut.has_value(), base::strConcat("a prefix of ", n, " bytes was accepted"));
		}
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
		ASSERT_EQUAL(plain.size() + ser::StreamHeader::SERIALIZED_SIZE, framed.size());

		const auto peeked = ser::peekHeader(view(framed), MAGIC);
		ASSERT_TRUE(peeked.has_value());
		ASSERT_EQUAL(framed.size(), usize(peeked->totalSize()));
		ASSERT_EQUAL(MAGIC, peeked->userMagic());

		constexpr u64 TREE_SCHEMA = ser::schemaHash<Tree>();
		ASSERT_EQUAL(TREE_SCHEMA, peeked->schema_hash);

		const ser::Options opts{ .header = true, .user_magic = MAGIC };
		ASSERT_TRUE(ser::read<Tree>(view(framed), opts).has_value());

		// 1. the envelope has to be there at all
		ASSERT_EQUAL(ser::Errc::Truncated, ser::codeOf(ser::read<Tree>(view(framed, 16), opts)));

		// 2. then the magic - somebody else's ser stream, and a damaged one
		ASSERT_EQUAL(
			ser::Errc::BadMagic, ser::codeOf(ser::read<Tree>(view(framed), { .header = true }))
		);
		ByteBuf damaged = framed;
		damaged[1]      = std::byte{ 'X' };
		ASSERT_EQUAL(ser::Errc::BadMagic, ser::codeOf(ser::read<Tree>(view(damaged), opts)));

		// 3. then the platform, which is checked before anything in the stream is believed
		damaged     = framed;
		damaged[28] = std::byte{ 0xFF };  // the low byte of `flags`
		ASSERT_EQUAL(ser::Errc::PlatformMismatch, ser::codeOf(ser::read<Tree>(view(damaged), opts)));

		// 4. and only then the schema, which is the one step that needs the type
		ASSERT_EQUAL(ser::Errc::SchemaMismatch, ser::codeOf(ser::read<Leaf>(view(framed), opts)));
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
		// representation never reaches the stream and what comes back is validated.
		ByteBuf flag;
		ASSERT_TRUE(ser::write(flag, true).has_value());
		ASSERT_EQUAL(usize{ 1 }, flag.size());
		flag[0] = std::byte{ 2 };
		ASSERT_EQUAL(ser::Errc::InvalidValue, ser::codeOf(ser::read<bool>(view(flag))));

		// The same key twice is a damaged stream, not a merge: keeping the first would
		// turn it into a map smaller than the one that was written.
		ByteBuf duplicate;
		(void) ser::write(duplicate, u64{ 2 });
		(void) ser::write(duplicate, u32{ 1 });
		(void) ser::write(duplicate, u32{ 10 });
		(void) ser::write(duplicate, u32{ 1 });
		(void) ser::write(duplicate, u32{ 20 });
		using Table = std::map<u32, u32>;
		ASSERT_EQUAL(ser::Errc::InvalidValue, ser::codeOf(ser::read<Table>(view(duplicate))));

		// A length prefix is the first thing an attacker reaches for: it arrives before
		// the elements it counts, so it is checked against what the stream can hold BEFORE
		// anything is reserved.
		ByteBuf lying;
		(void) ser::write(lying, u64{ 1'000 });
		using Numbers = std::vector<u32>;
		ASSERT_EQUAL(ser::Errc::Truncated, ser::codeOf(ser::read<Numbers>(view(lying))));
		ASSERT_EQUAL(ser::Errc::Truncated, ser::codeOf(ser::read<std::string>(view(lying))));

		// No ceiling is involved for an element that has bytes in the stream, and that is
		// deliberate: n * MIN_SERIALIZED_SIZE_V<E> bytes have to be there, which bounds the count
		// by the input itself and lets a legitimately huge container read.
		ByteBuf absurd;
		(void) ser::write(absurd, u64{ 1 } << 40);
		ASSERT_EQUAL(ser::Errc::Truncated, ser::codeOf(ser::read<Numbers>(view(absurd))));

		// An EMPTY element is the one case that argument cannot reach: any number of them
		// occupies no bytes, so the stream carries no evidence about the count and the
		// policy ceiling is the only bound there is.
		ByteBuf empties;
		(void) ser::write(empties, u64{ ser::ConfigGlobal::MAX_ZERO_SIZE_ELEMENTS } + 1);
		ASSERT_EQUAL(
			ser::Errc::MessageSize, ser::codeOf(ser::read<std::vector<Nothing>>(view(empties)))
		);

		// The buffer has to be used up. A record read as a SHORTER one leaves bytes over,
		// and that is the only signal there is that the two shapes disagree - the payload
		// itself is perfectly well-formed either way.
		ByteBuf pair;
		(void) ser::write(pair, u64{ 1 });
		(void) ser::write(pair, u64{ 2 });
		ASSERT_EQUAL(ser::Errc::TrailingBytes, ser::codeOf(ser::read<u64>(view(pair))));
		ASSERT_TRUE(ser::read<u64>(view(pair, sizeof(u64))).has_value());

		// The archive is the other half of that rule: reading several appended messages is
		// what it is for, so it counts nothing and says where it stopped.
		ser::In appended{ view(pair) };
		u64     one = 0;
		u64     two = 0;
		ASSERT_EQUAL(ser::Errc::Ok, appended(one, two));
		ASSERT_EQUAL(u64{ 1 }, one);
		ASSERT_EQUAL(u64{ 2 }, two);

		// A hook that reads with ser::subMake throws from inside a function declared to
		// return a code. Neither entry point may let that out: ser::read turns it into a
		// result, and the ARCHIVE - which is documented never to throw - into a code.
		ByteBuf half;
		(void) ser::write(half, i32{ 7 });
		ASSERT_EQUAL(ser::Errc::Truncated, ser::codeOf(ser::read<Thrower>(view(half))));

		Thrower target;
		ser::In throwing_ar{ view(half) };
		ASSERT_EQUAL(ser::Errc::Truncated, throwing_ar(target));

		ByteBuf whole;
		(void) ser::write(whole, Thrower{ .a = 3, .b = 4 });
		ASSERT_TRUE(SER_TEST_ROUNDTRIP(Thrower{ .a = 3, .b = 4 }));

		// The depth counter, checked against the configured limit rather than an
		// accidental one. The nesting is in the value, not in the type, so the compiler
		// never sees it.
		Node deep;
		for (usize i = 0; i < ser::ConfigGlobal::MAX_DEPTH + 8; ++i) {
			Node parent;
			parent.kids.push_back(std::move(deep));
			deep = std::move(parent);
		}
		ByteBuf  nested;
		ser::Out ar{ nested };
		ASSERT_EQUAL(ser::Errc::DepthExceeded, ar(deep));
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
		constexpr u64 ORDERED   = ser::schemaHash<std::map<u32, Leaf>>();
		constexpr u64 UNORDERED = ser::schemaHash<std::unordered_map<u32, Leaf>>();
		ASSERT_EQUAL(ORDERED, UNORDERED);

		// The element type is part of the format, and no sizeof of anybody's std::vector
		// enters into it.
		constexpr u64 OF_U32   = ser::schemaHash<std::vector<u32>>();
		constexpr u64 OF_FLOAT = ser::schemaHash<std::vector<float>>();
		assertTrue(OF_U32 != OF_FLOAT, "vector<u32> and vector<float> are not the same format");

		// A rename is not a format change, and that is the whole reason field names stay
		// out of the schema hash: the C++23 and C++26 backends must agree on it, and only
		// one of them knows the names. debugHash is where they go.
		constexpr u64 DESCRIBED_SCHEMA = ser::schemaHash<Described>();
		constexpr u64 RENAMED_SCHEMA   = ser::schemaHash<Renamed>();
		ASSERT_EQUAL(DESCRIBED_SCHEMA, RENAMED_SCHEMA);

		constexpr u64 DESCRIBED_DEBUG = ser::debugHash<Described>();
		constexpr u64 RENAMED_DEBUG   = ser::debugHash<Renamed>();
		assertTrue(DESCRIBED_DEBUG != RENAMED_DEBUG, "debugHash has to see the rename");

		// A lower bound and nothing else, but it has to be a truthful one: it is what
		// stands between a corrupt length prefix and an allocation.
		static_assert(
			ser::MIN_SERIALIZED_SIZE_V<Leaf> >= sizeof(i32) + sizeof(double) + 1 + sizeof(u16) + 4
		);
		static_assert(ser::MIN_SERIALIZED_SIZE_V<std::vector<Leaf>> >= sizeof(u64));
	}

	/**
	 * @brief A type that contains itself: it hashes, and it round-trips.
	 *
	 * The hash is the interesting half and it is pinned by the static_asserts above this
	 * class - a back-reference is a compile-time thing. What runs here is the other half:
	 * the same nesting really does survive serialization, and the depth guard is what stops a
	 * stream that claims more nesting than the reader will do.
	 */
	void recursiveTypes() {
		const Node tree{
			.value = 1,
			.kids  = { Node{ .value = 2, .kids = { Node{ .value = 3, .kids = {} } } },
			           Node{ .value = 4, .kids = {} } },
		};

		ByteBuf buf;
		ASSERT_TRUE(ser::write(buf, tree).has_value());

		const auto back = ser::read<Node>(view(buf));
		ASSERT_TRUE(back.has_value());
		assertTrue(back->value == tree, "the recursive value did not survive the round trip");

		// Through two types, and through an optional - the shapes the hash tells apart.
		const Branchy nested{ .value = 5,
			                  .twig  = Twig{ .branches = { Branchy{ .value = 6, .twig = {} } } } };
		ASSERT_TRUE(SER_TEST_ROUNDTRIP(nested));

		const Chain chain{ .value = 7,
			               .rest = std::vector<Chain>{ Chain{ .value = 8, .rest = std::nullopt } } };
		ASSERT_TRUE(SER_TEST_ROUNDTRIP(chain));

		// The envelope of a recursive type, end to end: the schema check is what the
		// back-reference feeds, so a stream of a Node is refused for a Chain.
		const ser::Options opts{ .header = true };
		ByteBuf            framed;
		ASSERT_TRUE(ser::write(framed, tree, opts).has_value());
		ASSERT_TRUE(ser::read<Node>(view(framed), opts).has_value());
		ASSERT_EQUAL(ser::Errc::SchemaMismatch, ser::codeOf(ser::read<Chain>(view(framed), opts)));

		// No prefix of it may be accepted, exactly as for any other type.
		for (usize n = 0; n < buf.size(); ++n)
			assertTrue(
				!ser::read<Node>(view(buf, n)).has_value(),
				base::strConcat("a prefix of ", n, " bytes was accepted")
			);

		// Data-dependent nesting is bounded by the depth guard rather than by the stack: a
		// chain deeper than MAX_DEPTH is refused on the way in.
		Node deep;
		{
			Node* tip = &deep;
			for (usize i = 0; i < ser::ConfigGlobal::MAX_DEPTH + 8; ++i)
				tip = &tip->kids.emplace_back();
		}
		ByteBuf  overflowing;
		ser::Out ar{ overflowing };
		ASSERT_EQUAL(ser::Errc::DepthExceeded, ar(deep));
		ASSERT_EQUAL(usize{ 0 }, ar.depth());

		// And on the way OUT, which is the direction that faces a disk: a chain nobody could
		// have written, built by hand rather than from an object, has to come back as a code
		// and not as a stack overflow. Twelve bytes per level - an i32 and a count of one -
		// and the last level closes with a count of zero.
		ByteBuf  handmade;
		ser::Out deep_ar{ handmade };
		for (usize i = 0; i < ser::ConfigGlobal::MAX_DEPTH + 64; ++i)
			ASSERT_EQUAL(ser::Errc::Ok, deep_ar(i32{ 1 }, u64{ 1 }));
		ASSERT_EQUAL(ser::Errc::Ok, deep_ar(i32{ 1 }, u64{ 0 }));

		const auto too_deep = ser::read<Node>(view(handmade));
		ASSERT_EQUAL(ser::Errc::DepthExceeded, ser::codeOf(too_deep));

		// The same shape within the limit still reads, so the guard is a limit and not a
		// refusal of nesting.
		ByteBuf  shallow;
		ser::Out shallow_ar{ shallow };
		for (usize i = 0; i < 10; ++i) ASSERT_EQUAL(ser::Errc::Ok, shallow_ar(i32{ 1 }, u64{ 1 }));
		ASSERT_EQUAL(ser::Errc::Ok, shallow_ar(i32{ 1 }, u64{ 0 }));
		ASSERT_TRUE(ser::read<Node>(view(shallow)).has_value());
	}

	/** @brief The last rung of the structured-bindings table still binds every member. */
	void memberCountLimit() {
		ByteBuf buf;
		ASSERT_TRUE(ser::write(buf, Wide64{}).has_value());
		ASSERT_EQUAL(::base::LADDER_MAX * sizeof(i32), buf.size());

		Wide64 sample;
		sample.f0  = -1;
		sample.f63 = 4'242;
		ByteBuf marked;
		ASSERT_TRUE(ser::write(marked, sample).has_value());

		const auto back = ser::read<Wide64>(view(marked));
		ASSERT_TRUE(back.has_value());
		ASSERT_EQUAL(i32{ -1 }, back->value.f0);
		ASSERT_EQUAL(i32{ 4'242 }, back->value.f63);
		// The middle of the chain, so a rung dropped anywhere shows up here.
		ASSERT_EQUAL(i32{ 32 }, back->value.f32);
	}

	/** @brief A const T reads as T, for a scalar and an aggregate alike. */
	void constQualifiedRead() {
		ByteBuf scalar;
		ASSERT_TRUE(ser::write(scalar, i32{ -7 }).has_value());
		const auto i = ser::read<const i32>(view(scalar));
		static_assert(std::is_same_v<std::remove_cvref_t<decltype(i->value)>, i32>);
		ASSERT_TRUE(i.has_value());
		ASSERT_EQUAL(i32{ -7 }, i->value);
		ASSERT_EQUAL(i32{ -7 }, ser::readOrPanicForce<const i32>(view(scalar)));

		ByteBuf aggregate;
		ASSERT_TRUE(ser::write(aggregate, Wide64{}).has_value());
		const auto w = ser::readOrPanic<const Wide64>(view(aggregate));
		ASSERT_EQUAL(i32{ 63 }, w->f63);
	}

	/** @brief std::array and a C array are the same bytes and the same schema. */
	void arrays() {
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
		using CArray = u32[3];
		static_assert(ser::schemaHash<std::array<u32, 3>>() == ser::schemaHash<CArray>());
		static_assert(
			ser::schemaHash<std::array<u32, 3>>() != ser::schemaHash<std::array<u32, 4>>()
		);
		static_assert(ser::MIN_SERIALIZED_SIZE_V<CArray> == 3 * sizeof(u32));

		ByteBuf from_std;
		ASSERT_TRUE(ser::write(from_std, std::array<u32, 3>{ 1, 2, 3 }).has_value());
		ByteBuf      from_c;
		const CArray c_array = { 1, 2, 3 };
		ASSERT_TRUE(ser::write(from_c, c_array).has_value());
		ASSERT_TRUE(from_std == from_c);

		ByteBuf frozen;
		ASSERT_TRUE(ser::write(frozen, FrozenWithArray{ .key = 7, .codes = { 4, 5, 6 } }).has_value()
		);
		const auto back = ser::readOrPanic<FrozenWithArray>(view(frozen));
		ASSERT_EQUAL(i32{ 7 }, back->key);
		ASSERT_EQUAL(u16{ 6 }, back->codes[2]);
	}

	// helpers

	/** @brief Round-trips one alternative and checks the tag it came back under. */
	template<class V>
	void expectAlternative(const V& sample, usize index) {
		ByteBuf buf;
		assertTrue(ser::write(buf, sample).has_value(), "a variant failed to write");
		const auto back = ser::read<V>(view(buf));
		assertTrue(back.has_value(), "a variant failed to read back");
		assertTrue(
			back->value.index() == index,
			base::strConcat("alternative ", index, " came back as ", back->value.index())
		);
		assertTrue(back->value == sample, "the alternative did not survive the round trip");
	}

	/** @brief Writes `sample`, checks which rung produced the bytes, and reads it back. */
	template<uchar M, class T>
	void expectRung(const T& sample, usize serialized_size, std::string_view what) {
		ByteBuf buf;
		assertTrue(ser::write(buf, sample).has_value(), base::strConcat("write failed: ", what));
		assertTrue(
			buf.size() == serialized_size,
			base::strConcat(
				"wrong serialized size for ", what, ": ", buf.size(), " != ", serialized_size
			)
		);
		assertTrue(buf[0] == std::byte{ M }, base::strConcat("a different rung handled ", what));

		const auto back = ser::read<T>(view(buf));
		assertTrue(back.has_value(), base::strConcat("read failed: ", what));
		assertTrue(back->value == sample, base::strConcat("value changed: ", what));
	}
};

TESTER_COMMON_MAIN("/src/common/ser/tests/");
