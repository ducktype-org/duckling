/**
 * @file ser_example.cpp
 * @brief How to use `ser`, from the simplest type to the most demanding one. Every
 * section is a type followed by the function that writes it, reads it back and compares
 * the two - so the whole use of the library is visible in one place.
 *
 *    1-2  types that need no serialization code at all
 *    3-5  the macros: ser_members<N>, SER_DESCRIBE, SER_DESCRIBE_MAKE
 *    6-8  hooks written by hand: serVisit, serWrite + serRead, serWrite + serMake
 *   9-10  types we do not own: ADL serVisit, ser::serializer<T>
 *  11-13  the stream: bad input, the envelope, several messages in one buffer
 *     14  at compile time: schemaHash, debugHash, a round trip inside a static_assert
 *
 * The rules behind all of it are in src/common/ser/readme.md.
 *
 *   cd build && ninja ser_example && ./bin/ser_example
 */

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <ser/base/all.hpp>  // opt-in: base::Optional, base::Box, base::Map, ...
#include <ser/macros.hpp>
#include <ser/ser.hpp>
#include <ser/std/all.hpp>  // opt-in: std::string, std::vector, std::map, ...
#include <ser/test.hpp>     // SER_TEST_ROUNDTRIP

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ═══ 1. a plain aggregate ═════════════════════════════════════════════════════════
// Public fields, found through structured bindings and written in declaration order.
// Nothing to declare, register or list.

struct Point final {
	i32 x = 0;
	i32 y = 0;
};

void plainAggregateExample() {
	std::println("\n── 1. a plain aggregate, with no serialization code ───────────────");

	const Point point{ .x = 3, .y = -4 };

	// Writing an object we are already holding cannot fail unless the program is broken,
	// so this is the panicking form. It APPENDS to the buffer.
	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, point);

	// The bytes are ours too - written a line ago - so the read may panic as well.
	const auto back = ser::readOrPanicForce<Point>(bytes);

	std::println(
		"  {} bytes; back ({}, {}), equal: {}",
		bytes.size(),
		back.x,
		back.y,
		back.x == point.x && back.y == point.y
	);
}

// ═══ 2. containers, enums, std and base types ═════════════════════════════════════
// Still no serialization code - only the two opt-in headers included at the top, which
// is what teaches ser about the std and base types.

enum class Color : std::uint8_t { Red = 1, Blue = 2 };  // travels as its underlying type

struct Person final {
	std::string                 name;
	Color                       color = Color::Red;
	std::array<Point, 2>        pins;      // length is in the TYPE, so it is not written
	std::vector<i32>            scores;    // container: u64 length, then the elements
	std::optional<std::string>  nickname;  // presence byte, then the value if present
	std::map<std::string, i32>  debts;     // ordered, so equal maps give equal bytes
	base::Optional<std::string> note;      // base: byte for byte a std::optional
	base::Box<Point>            home;      // the pointer is storage, not format: a Point
};

// The static analyzer does not model base::Box's deleter and reads makeBox as a leak.
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
Person samplePerson() {
	return Person{
		.name     = "Ada",
		.color    = Color::Blue,
		.pins     = { Point{ .x = 1, .y = 2 }, Point{ .x = 3, .y = 4 } },
		.scores   = { 10, 20, 30 },
		.nickname = "the countess",
		.debts    = { { "Babbage", 7 }, { "Byron", -2 } },
		.note     = "likes engines",
		.home     = makeBox<Point>(0, 0),
	};
}

void containersExample() {
	std::println("\n── 2. containers, enums, std and base types ───────────────");

	const Person person = samplePerson();

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, person);

	const auto back = ser::readOrPanicForce<Person>(bytes);

	std::println(
		"  {} bytes; back {}, {} scores, nickname {}, note {}, home ({}, {})",
		bytes.size(),
		back.name,
		back.scores.size(),
		*back.nickname,
		*back.note,
		back.home->x,
		back.home->y
	);
	std::println(
		"  every field equal: {}",
		back.name == person.name && back.color == person.color && back.scores == person.scores
			&& back.debts == person.debts
	);
}

// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

// ═══ 3. ser_members<N>: a field the walk cannot count ═════════════════════════════
// A C array field elides braces, so the probe that counts fields by trying aggregate
// initialization answers 5 for these 2. Saying the number is the whole fix, and the type
// then serializes like any other. A wrong N is a compile error, never wrong bytes.

struct Marker final {
	// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	char code[4] = { 'S', 'E', 'R', '!' };
	u32  count   = 0;

	using ser_members = ser::members<2>;
};

void memberCountExample() {
	std::println("\n── 3. ser_members<N>, for a field the walk cannot count ───────────────");

	const Marker marker{ .code = { 'D', 'U', 'C', 'K' }, .count = 7 };

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, marker);

	const auto back = ser::readOrPanicForce<Marker>(bytes);

	std::println(
		"  {} bytes; back \"{}\" x{}, equal: {}",
		bytes.size(),
		std::string_view{ static_cast<const char*>(back.code), 4 },
		back.count,
		std::string_view{ static_cast<const char*>(back.code), 4 }
				== std::string_view{ static_cast<const char*>(marker.code), 4 }
			&& back.count == marker.count
	);
}

// ═══ 4. SER_DESCRIBE: naming the fields that go on the wire ═══════════════════════
// Two reasons here, either enough alone: the fields are PRIVATE, which the walk cannot
// probe (SER_FRIEND is what lets ser look), and `length` is derived from `text`, so it has
// no business on the wire. A field left out of the list comes back default-built.

class Message final {
public:
	Message() = default;  // serVisit FILLS an object, so there has to be one to fill

	explicit Message(std::string text): text(std::move(text)), length(this->text.size()) {}

	[[nodiscard]] const std::string& body() const { return text; }

	[[nodiscard]] usize cachedLength() const { return length; }

SER_FRIEND  // = friend struct ser::access; - without it the macro below is invisible

	private: std::string text;
	usize                length = 0;

	SER_DESCRIBE(text)  // the wire is exactly this one field
};

void describeExample() {
	std::println("\n── 4. SER_DESCRIBE, for private fields and fields to skip ───────────────");

	const Message message{ "hello ser" };

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, message);

	const auto back = ser::readOrPanicForce<Message>(bytes);

	std::println(
		"  {} bytes ({} of them the text); back \"{}\", equal: {}",
		bytes.size(),
		message.body().size(),
		back.body(),
		back.body() == message.body()
	);
	std::println(
		"  the skipped `length` came back default-built: {} (was {})",
		back.cachedLength(),
		message.cachedLength()
	);
}

// ═══ 5. SER_DESCRIBE_MAKE: a type that has to be BUILT ════════════════════════════
// `id` is const and there is no default constructor, so a read cannot make an empty object
// and overwrite its fields. The macro emits serWrite plus serMake, and serMake returns the
// object by value - built once, at its final address.

class Measurement final {
public:
	Measurement(u32 id, float value): id(id), value(value) {}

	[[nodiscard]] u32 sensor() const { return id; }

	[[nodiscard]] float reading() const { return value; }

	SER_FRIEND

private:
	const u32 id;
	float     value;

	SER_DESCRIBE_MAKE(Measurement, id, value)
};

void describeMakeExample() {
	std::println("\n── 5. SER_DESCRIBE_MAKE, when the read has to BUILD the object ───────────────");

	const Measurement measurement{ 42, 3.5F };

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, measurement);

	const auto back = ser::readOrPanicForce<Measurement>(bytes);

	std::println(
		"  {} bytes; back sensor {} value {}, sensor equal: {}",
		bytes.size(),
		back.sensor(),
		back.reading(),
		back.sensor() == measurement.sensor()
	);

	// Two fields of the SAME type swapped in the macro's list compiles and writes the wrong
	// bytes - no hash can see that, so this round-trip check is what does.
	std::println("  SER_TEST_ROUNDTRIP: {}", SER_TEST_ROUNDTRIP(Measurement{ 7, 1.5F }));
}

// ═══ 6. serVisit: one hook, both directions ═══════════════════════════════════════
// What SER_DESCRIBE generates, written out. One body serves both because `ar(...)` gets a
// const reference when writing and a mutable one when reading. The order is pinned in the
// hook, so reordering the fields below cannot change the format by accident. `constexpr`
// is not required - it is what lets section 14 round-trip this type during the build.

struct Timestamp final {
	i64 seconds = 0;
	i32 nanos   = 0;

	static constexpr ser::Errc serVisit(auto& ar, auto& self) {
		return ar(self.seconds, self.nanos);
	}
};

void visitHookExample() {
	std::println("\n── 6. serVisit, one hook for both directions ───────────────");

	const Timestamp stamp{ .seconds = 1'700'000'000, .nanos = 250 };

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, stamp);

	const auto back = ser::readOrPanicForce<Timestamp>(bytes);

	std::println(
		"  {} bytes; back {}.{}, equal: {}",
		bytes.size(),
		back.seconds,
		back.nanos,
		back.seconds == stamp.seconds && back.nanos == stamp.nanos
	);
}

// ═══ 7. serWrite + serRead: the asymmetric pair ═══════════════════════════════════
// Bytes from an older build arrive on the read side, so validation belongs there. A version
// field plus a check is also the whole of schema evolution in ser - there are no optional
// or defaulted fields.

struct Config final {
	u32             version = 2;
	std::vector<u8> data;

	static ser::Errc serWrite(ser::writer auto& ar, const Config& x) {
		return ar(x.version, x.data);
	}

	static ser::Errc serRead(ser::reader auto& ar, Config& x) {
		if (const auto e = ar(x.version); e != ser::Errc::Ok) return e;
		if (x.version != 2) return ser::Errc::InvalidValue;  // our own rule, mid-stream
		return ar(x.data);
	}
};

void writeReadHookExample() {
	std::println("\n── 7. serWrite + serRead, with validation on the way in ───────────────");

	const Config config{ .version = 2, .data = { u8{ 1 }, u8{ 2 }, u8{ 3 } } };

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, config);

	const auto back = ser::readOrPanicForce<Config>(bytes);
	std::println(
		"  {} bytes; back version {}, {} bytes of data, equal: {}",
		bytes.size(),
		back.version,
		back.data.size(),
		back.data == config.data
	);

	// A config from a build that wrote version 1. These bytes are not ours to trust, so
	// ser::read - which hands back a code and the position that failed - and not a panic.
	std::vector<std::byte> old_bytes;
	ser::writeOrPanic(old_bytes, Config{ .version = 1, .data = {} });

	const auto older = ser::read<Config>(old_bytes);
	std::println("  a config from version 1: {}", older ? "accepted" : older.err().message());
}

// ═══ 8. serWrite + serMake: building by hand ══════════════════════════════════════
// The same job as SER_DESCRIBE_MAKE, where a field list cannot say it: the public
// constructor has a different shape than the member. `ser::readField<F>` pulls one field
// off the stream, and braces are what guarantee several of them happen in order.

class Label final {
public:
	Label(std::string_view colour, u32 number): text(std::format("{}-{}", colour, number)) {}

	[[nodiscard]] const std::string& printed() const { return text; }

	SER_FRIEND

private:
	explicit Label(std::string printed): text(std::move(printed)) {}

	std::string text;

	static ser::Errc serWrite(ser::writer auto& ar, const Label& x) { return ar(x.text); }

	static Label serMake(ser::reader auto& ar) { return Label{ ser::readField<std::string>(ar) }; }
};

void makeHookExample() {
	std::println("\n── 8. serWrite + serMake, written out by hand ───────────────");

	const Label label{ "sky", 42 };

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, label);

	const auto back = ser::readOrPanicForce<Label>(bytes);

	std::println(
		"  {} bytes; back \"{}\", equal: {}",
		bytes.size(),
		back.printed(),
		back.printed() == label.printed()
	);
}

// ═══ 9. a type in someone else's namespace: ADL ═══════════════════════════════════

namespace vendor {

	/** @brief Their header: we may add functions next to it, but not members. */
	struct Coord final {
		double lat = 0;
		double lon = 0;
	};

	// BOTH overloads: the const one is what the write side needs, and a hook found in one
	// direction only means the two directions disagree about the format, which ser refuses.
	template<class Ar>
	ser::Errc serVisit(Ar& ar, Coord& c) {
		return ar(c.lat, c.lon);
	}

	template<class Ar>
	ser::Errc serVisit(Ar& ar, const Coord& c) {
		return ar(c.lat, c.lon);
	}

	/** @brief And a header we cannot touch at all - not even to add a free function. */
	struct Handle final {
		u32 id = 0;
	};

}  // namespace vendor

void adlExample() {
	std::println("\n── 9. someone else's type, through ADL ───────────────");

	const vendor::Coord coord{ .lat = 52.23, .lon = 21.01 };

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, coord);

	const auto back = ser::readOrPanicForce<vendor::Coord>(bytes);

	std::println("  {} bytes; back ({}, {})", bytes.size(), back.lat, back.lon);
}

// ═══ 10. a type we cannot edit at all: ser::serializer<T> ═════════════════════════
// The last resort and the highest rank: it outranks every hook, and it lives in OUR code,
// so nothing in the vendor's headers has to change.

namespace ser {

	template<>
	struct serializer<::vendor::Handle> {
		static constexpr Errc visit(auto& ar, auto& self) { return ar(self.id); }
	};

}  // namespace ser

void serializerExample() {
	std::println("\n── 10. a sealed type, through ser::serializer<T> ───────────────");

	const vendor::Handle handle{ .id = 7 };

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, handle);

	const auto back = ser::readOrPanicForce<vendor::Handle>(bytes);

	std::println("  {} bytes; back id {}, equal: {}", bytes.size(), back.id, back.id == handle.id);
}

// ═══ 11. bytes that are allowed to be wrong ═══════════════════════════════════════
// Input - a file, a cache, a socket - goes through ser::read, which hands back a code and
// the position that failed. Never a panicking form there: CORE_PANIC is std::unreachable()
// in a Release build, so a bad stream would be undefined behaviour with no diagnostic.

void badInputExample() {
	std::println("\n── 11. reading input that is allowed to be wrong ───────────────");

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, samplePerson());

	const auto cut = ser::read<Person>(std::span{ bytes }.first(bytes.size() / 2));
	std::println("  half a Person: {}", cut ? "accepted" : cut.err().message());

	// The first eight bytes are the name's length. A corrupt length is caught before
	// anything is allocated - a stream is untrusted input, not a plan for a malloc.
	std::vector<std::byte> corrupt = bytes;
	for (usize i = 0; i < 8; ++i) corrupt[i] = std::byte{ 0xFF };

	const auto huge = ser::read<Person>(corrupt);
	std::println("  a name of 2^64 bytes: {}", huge ? "accepted" : huge.err().message());
}

// ═══ 12. the envelope ═════════════════════════════════════════════════════════════
// Thirty-two bytes in front of the payload, so a stream from another build, another byte
// order or another type is refused instead of interpreted. Off by default, so a plain
// write is exactly the payload and not one byte more.

void envelopeExample() {
	std::println("\n── 12. the envelope, 32 bytes that refuse the wrong stream ───────────────");

	constexpr ser::options OPT{ .header = true, .user_magic = 0xD0'CC'00'01U };

	std::vector<std::byte> plain;
	ser::writeOrPanic(plain, samplePerson());

	std::vector<std::byte> framed;
	ser::writeOrPanic(framed, samplePerson(), OPT);
	std::println("  {} bytes plain, {} bytes framed", plain.size(), framed.size());

	const auto same = ser::read<Person>(framed, OPT);
	std::println("  read back as a Person: {}", same ? same->value.name : same.err().message());

	const auto wrong = ser::read<Point>(framed, OPT);
	std::println("  read back as a Point:  {}", wrong ? "accepted" : wrong.err().message());
}

// ═══ 13. several messages in one buffer ═══════════════════════════════════════════
// ser::write APPENDS, so several objects share one buffer. Reading them back is one
// ARCHIVE and not several calls to ser::read: an archive says how far it got, while
// ser::read always starts at the front of the span it is given.

void archiveExample() {
	std::println("\n── 13. several messages in one buffer ───────────────");

	std::vector<std::byte> bytes;
	ser::writeOrPanic(bytes, Marker{ .code = { 'H', 'E', 'A', 'D' }, .count = 3 });
	ser::writeOrPanic(bytes, Point{ .x = 8, .y = 9 });

	ser::in ar{ std::span<const std::byte>{ bytes } };
	Marker  marker;
	Point   point;

	if (ar(marker) != ser::Errc::Ok || ar(point) != ser::Errc::Ok) {
		std::println("  the archive stopped at position {}", ar.position());
		return;
	}

	std::println(
		"  {} bytes read as \"{}\" x{}, then ({}, {}); {} bytes left",
		ar.position(),
		std::string_view{ static_cast<const char*>(marker.code), 4 },
		marker.count,
		point.x,
		point.y,
		ar.avail()
	);
}

// ═══ 14. at compile time ══════════════════════════════════════════════════════════
// Nothing here runs when the program does: the hashes are consteval, and the bytes
// themselves are written and read back inside constant expressions. The function at the
// bottom only reports numbers the compiler had already decided.

struct Coords final {
	i32 first  = 0;
	i32 second = 0;
	SER_DESCRIBE(first, second)
};

struct CoordsRenamed final {
	i32 first = 0;
	i32 other = 0;
	SER_DESCRIBE(first, other)
};

// schemaHash means "this is the format I write", and it hashes the WIRE - so a rename is
// not a format change, and only the diagnostics hash sees it. Pin the relations you rely
// on; the number itself is per toolchain and not portable.
constexpr bool SAME_WIRE   = ser::schemaHash<Coords>() == ser::schemaHash<CoordsRenamed>();
constexpr bool RENAME_SEEN = ser::debugHash<Coords>() != ser::debugHash<CoordsRenamed>();
static_assert(SAME_WIRE, "the two types have to agree on the format");
static_assert(RENAME_SEEN, "and debugHash is what notices a renamed field");
static_assert(ser::schemaHash<Point>() != ser::schemaHash<Person>());

/** @brief A whole round trip during compilation: no heap, so a fixed-size buffer. */
consteval bool roundTripWhileCompiling() {
	std::array<std::byte, 12> buf{};  // a fixed buffer: its size() is CAPACITY
	ser::out                  out{ buf };
	if (out(Timestamp{ .seconds = 1'700'000'000, .nanos = 250 }) != ser::Errc::Ok) return false;
	if (out.finish() != ser::Errc::Ok) return false;

	ser::in   in{ std::span<const std::byte>{ buf } };
	Timestamp back;
	if (in(back) != ser::Errc::Ok) return false;
	return back.seconds == 1'700'000'000 && back.nanos == 250 && in.position() == buf.size();
}

/** @brief And a fixed buffer that cannot hold the object answers with a code, not a resize. */
consteval bool refusesToOverflow() {
	std::array<std::byte, 4> tiny{};  // a Timestamp needs 12
	ser::out                 ar{ tiny };
	return ar(Timestamp{}) == ser::Errc::BufferFull;
}

constexpr bool ROUND_TRIPPED = roundTripWhileCompiling();
constexpr bool REFUSED_TINY  = refusesToOverflow();
static_assert(ROUND_TRIPPED, "write and read are constant-evaluable end to end");
static_assert(REFUSED_TINY, "and a fixed buffer that cannot hold the object says so");

void compileTimeExample() {
	std::println("\n── 14. at compile time, all of it decided during the build ───────────────");
	std::println("  schemaHash survives a rename:     {}", SAME_WIRE);
	std::println("  debugHash notices the rename:     {}", RENAME_SEEN);
	std::println("  round trip in a constant expr:    {}", ROUND_TRIPPED);
	std::println("  a 12 B object into a 4 B buffer:  {}", REFUSED_TINY ? "BufferFull" : "??");
}

int main() {
	std::println("ser example - library version {}", ser::VERSION);
	plainAggregateExample();
	containersExample();
	memberCountExample();
	describeExample();
	describeMakeExample();
	visitHookExample();
	writeReadHookExample();
	makeHookExample();
	adlExample();
	serializerExample();
	badInputExample();
	envelopeExample();
	archiveExample();
	compileTimeExample();
	std::println("");
}

// ── what ser refuses, and what to write instead ───────────────────────────────────
// None of these means anything outside the writing process, so each is a compile error
// naming the fix rather than bytes nobody can read back. Paste one in and read it.
//
//   Point*, Point&, void*   the value itself, or an index into a table you own
//   std::string_view        the owning type - a view cannot be read back into
//   base::SharedBox         write the objects once, store an index per holder
//   a union                 a tag next to the payload and a serVisit that switches on it
//   std::vector<bool>       std::vector<u8>, or std::bitset<N>
//
//   struct HasAPointer { Point* point; };
//   struct MiscountsItself { char code[4]; u32 n; };   // ser_members<2>, as Marker has
