/**
 * @file ser_playground.cpp
 * @brief A sandbox for `ser` - poke it, break it, print the bytes.
 *
 * Not a test: nothing here asserts anything. Every section writes something awkward, then
 * prints what came out and what ser said about it. The rules are in ser's own readme.
 *
 *   cd build && ninja ser_playground && ./bin/playgrounds/ser_playground
 *
 * Worth trying by hand: add a field to Duck and watch every hash move, or move a line out
 * of the BROKEN block at the bottom and read the compile error.
 */

#include <base/types/ints.hpp>

#include <query_framework/query_errors.hpp>
#include <query_framework/query_result.hpp>
#include <ser/macros.hpp>
#include <ser/ser.hpp>
#include <ser/std/all.hpp>

#include <cstddef>
#include <format>
#include <map>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using Buf = std::vector<std::byte>;

/** @brief The whole buffer, or its first `n` bytes for the truncated cases. */
std::span<const std::byte> view(const Buf& b, usize n = ~usize{ 0 }) {
	return { b.data(), n < b.size() ? n : b.size() };
}

/** @brief One object's bytes, so that a demo line can be one line. */
template<class T>
Buf bytesOf(const T& x, ser::options opt = {}) {
	Buf b;
	(void) ser::write(b, x, opt);
	return b;
}

void heading(std::string_view what) { std::println("\n── {} ───────────────", what); }

/** @brief Size and the first bytes in hex - the wire is small enough to read by eye. */
void dump(std::string_view label, const Buf& b) {
	std::string hex;
	for (usize i = 0; i < b.size() && i < 20; ++i)
		hex += std::format("{:02x} ", std::to_integer<unsigned>(b[i]));
	std::println("  {:<26}{:>4} B  {}{}", label, b.size(), hex, b.size() > 20 ? "..." : "");
}

/** @brief Whatever ser thought of a call, in one line. */
template<class R>
void say(std::string_view label, const R& r) {
	std::println("  {:<26}{}", label, r.hasValue() ? "ok" : r.err().message());
}

// ═══ the menagerie ═════════════════════════════════════════════════════════════
// Everything here round-trips, and the plain aggregates needed no code at all for it.
enum class Plumage : u16 { Fluffy = 1, Sleek = 2, Scandalous = 60'000 };

/** @brief One quack. A public aggregate: no serialization code whatsoever. */
struct Quack final {
	std::string sound   = "quack";
	u8          volume  = u8{ 3 };  // u8 is a strong typedef, and needs no code either
	float       seconds = 0.5F;
};

/** @brief A duck, and one of every std adapter worth looking at. */
struct Duck final {
	std::string                                    name;
	Plumage                                        plumage = Plumage::Fluffy;
	std::vector<Quack>                             repertoire;
	std::optional<std::string>                     alias;        // presence byte, then value
	std::map<std::string, i32>                     bread_debts;  // ordered: stable bytes
	std::variant<std::monostate, i32, std::string> ring;         // u64 tag, then the value
};

/**
 * @brief A ringed duck: a `const` field, no default constructor, all of it private. A
 * `const` field cannot be filled in afterwards, so the read BUILDS the object instead
 * of overwriting one - which is what SER_DESCRIBE_MAKE emits.
 */
class Ringed final {
public:
	Ringed(u32 ring, std::string colour): ring_id(ring), band(std::move(colour)) {}

	[[nodiscard]] std::string describe() const { return std::format("#{} {}", ring_id, band); }

	SER_FRIEND

private:
	const u32   ring_id;
	std::string band;
	SER_DESCRIBE_MAKE(Ringed, ring_id, band)
};

/**
 * @brief A leg tag, and the one shape the field count gets wrong: `char[4]` elides
 * braces, so the probe answers 5 fields for these 2. A wrong count is a compile error.
 */
struct Tag final {
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	char letters[4]   = { 'D', 'U', 'C', 'K' };
	u32  flock        = 0;
	using ser_members = ser::members<2>;
};

/** @brief A tape of quacks from an older program: the read refuses the wrong era. */
struct QuackTape final {
	u32             version = 2;
	std::vector<u8> samples;

	static ser::Errc serWrite(ser::writer auto& ar, const QuackTape& x) {
		return ar(x.version, x.samples);
	}

	static ser::Errc serRead(ser::reader auto& ar, QuackTape& x) {
		if (const auto e = ar(x.version); e != ser::Errc::Ok) return e;
		if (x.version != 2) return ser::Errc::InvalidValue;  // validation, mid-stream
		return ar(x.samples);
	}
};

/** @brief Contains itself, which is how a stream asks for unbounded recursion. */
struct Nest final {
	u8                twigs = u8{ 1 };
	std::vector<Nest> inside;
};

Nest deepNest(int depth) {
	Nest n;
	if (depth > 0) n.inside.push_back(deepNest(depth - 1));
	return n;
}

/** @brief A described type, and the same fields with one renamed. */
struct Described final {
	i32 wings = 2;
	i32 bills = 1;
	SER_DESCRIBE(wings, bills)
};

struct Renamed final {
	i32 wings   = 2;
	i32 beakies = 1;
	SER_DESCRIBE(wings, beakies)
};

Duck sampleDuck() {
	return Duck{
		.name        = "Feathers McGraw",
		.plumage     = Plumage::Scandalous,
		.repertoire  = { Quack{}, Quack{ .sound = "QUAAACK", .volume = u8{ 11 } } },
		.alias       = "the penguin",
		.bread_debts = { { "swan", -4 }, { "goose", 12 } },
		.ring        = std::string{ "gold" },
	};
}

// ═══ a query result, without asking the query framework ════════════════════════════
// A query declared IMPLEMENT_QUERY(Q, query::QResult<Duck>) has query::QResult<Duck> as
// its PResult, and nothing serializes a QResult today - its one member is a private
// variant, so the automatic walk cannot see it either. Ten lines over the public API and
// any query result over a serializable value can go to disk. `make` rather than `read`,
// because the object is BUILT from the stream instead of being overwritten.
namespace ser {

	template<class V>
	struct serializer<::query::QResult<V>> {
		static Errc write(writer auto& ar, const ::query::QResult<V>& self) {
			const bool has = self.hasValue();
			if (const auto e = ar(has); e != Errc::Ok) return e;
			return has ? ar(self.valueOrPanic()) : Errc::Ok;
		}

		static ::query::QResult<V> make(reader auto& ar) {
			if (!readField<bool>(ar)) return ::query::Failed{};
			return ::query::QResult<V>{ readField<V>(ar) };
		}
	};

}  // namespace ser

void theMenagerie() {
	heading("round trips, and what they weigh");

	struct Nothing final {};  // the funny one: its whole wire form is its absence

	const Buf duck   = bytesOf(sampleDuck());
	const Buf ringed = bytesOf(Ringed{ 42, "sky blue" });
	dump("a whole duck", duck);
	dump("Ringed, a const field", ringed);
	dump("Tag, char[4] and a u32", bytesOf(Tag{ .flock = 7 }));
	dump("an empty struct", bytesOf(Nothing{}));
	const Duck d = ser::readOrPanicForce<Duck>(view(duck));
	std::println("  {} quacks {} ways, ring #{}", d.name, d.repertoire.size(), d.ring.index());
	std::println("  built by serMake: {}", ser::readOrPanic<Ringed>(view(ringed))->describe());
}

void hashes() {
	heading("hashes: what the number can and cannot see");
	std::println("  Duck       {:#018x}", ser::schemaHash<Duck>());
	std::println("  Quack      {:#018x}", ser::schemaHash<Quack>());
	std::println("  Nest       {:#018x}  itself, and finite", ser::schemaHash<Nest>());
	std::println("  QuackTape  {:#018x}  sizeof and alignof only", ser::schemaHash<QuackTape>());

	// A rename is not a format change, and the two hashes disagree about that on purpose.
	constexpr bool SCHEMA_MOVED = ser::schemaHash<Described>() != ser::schemaHash<Renamed>();
	constexpr bool DEBUG_MOVED  = ser::debugHash<Described>() != ser::debugHash<Renamed>();
	std::println("  a rename: schema moved {}, debug moved {}", SCHEMA_MOVED, DEBUG_MOVED);
}

void envelopes() {
	heading("with the hash in front, and without");
	constexpr u32          MAGIC = 0xD0'CC'00'01U;
	constexpr ser::options OPTS{ .header = true, .user_magic = MAGIC };
	const Buf              plain  = bytesOf(sampleDuck());
	const Buf              framed = bytesOf(sampleDuck(), OPTS);
	dump("no envelope", plain);
	dump("32 bytes of envelope", framed);
	const auto peeked = ser::peekHeader(view(framed), MAGIC);
	std::println("  peeked with no type: schema {:#018x}", peeked->schema_hash);

	// The point of those thirty-two bytes: a stream is refused instead of interpreted.
	say("read<Duck>(framed)", ser::read<Duck>(view(framed), OPTS));
	say("read<Quack>(framed)", ser::read<Quack>(view(framed), OPTS));
	say("plain bytes, framed read", ser::read<Duck>(view(plain), OPTS));
	say("framed bytes, plain read", ser::read<Duck>(view(framed)));
}

void brokenStreams() {
	heading("streams that are wrong on purpose");
	const Buf duck = bytesOf(sampleDuck());
	say("cut in half", ser::read<Duck>(view(duck, duck.size() / 2)));

	// The first eight bytes are the name's length. A corrupt length is caught before
	// anything is allocated: a stream is untrusted input, not a plan for a malloc.
	Buf huge = duck;
	for (usize i = 0; i < 8; ++i) huge[i] = std::byte{ 0xFF };
	say("a name of 2^64 bytes", ser::read<Duck>(view(huge)));

	const Buf tape = bytesOf(QuackTape{ .version = 7, .samples = { u8{ 1 }, u8{ 2 } } });
	say("a tape from 1997", ser::read<QuackTape>(view(tape)));

	// Recursion has a ceiling, and it holds on the WRITE side too - otherwise a deep
	// enough object would take the stack out before it ever became a stream.
	Buf too_deep;
	say("a nest 300 deep", ser::write(too_deep, deepNest(300)));
	dump("40 nests of one twig", bytesOf(deepNest(40)));
}

void queryResults() {
	heading("a query result, serialized by nobody's request");
	using DuckResult = query::QResult<Duck>;  // the PResult of a query returning a Duck
	const Buf ok     = bytesOf(DuckResult{ sampleDuck() });
	const Buf failed = bytesOf(DuckResult{ query::Failed{} });
	dump("QResult<Duck>, a duck", ok);
	dump("QResult<Duck>, Failed", failed);
	const auto duck_back   = ser::readOrPanicForce<DuckResult>(view(ok));
	const auto failed_back = ser::readOrPanicForce<DuckResult>(view(failed));
	std::println("  the duck came back as {}", duck_back.valueOrPanic().name);
	std::println("  the failure came back: {}", failed_back.hasFailed());
	std::println("  schemaHash<QResult<Duck>> {:#018x}", ser::schemaHash<DuckResult>());
}

int main() {
	std::println("ser playground - library version {}", ser::VERSION);
	theMenagerie();
	hashes();
	envelopes();
	brokenStreams();
	queryResults();
	std::println("");
}

// ── BROKEN on purpose ────────────────────────────────────────────────────────────
// Turn the block on and the build stops with a message that names the fix, because a
// refusal fires where a type is USED - so it takes the write below, not just the field.
#if 0
struct HasAPointer { Duck* duck; };              // write the value, or an index
struct HasAReference { Duck& duck; };            // a reference can never be rebound
struct HasAView { std::string_view name; };      // a view cannot be read back into
struct MiscountsItself { char tag[4]; u32 n; };  // the char[4] probe, uncorrected
void  breakTheBuild() { (void) bytesOf(HasAPointer{ nullptr }); }
#endif
