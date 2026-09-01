/**
 * @file ser_base_test.cpp
 *
 * The adapters for the base types: <ser/base/all.hpp>. Kept apart from ser_test.cpp
 * because it is a different question - that suite pins the library's own rules, this one
 * pins what each base type does on the wire, which types are refused, and which streams
 * are interchangeable with their std counterparts.
 *
 * The refusals cannot be tested from here: a static_assert that fires is a build failure,
 * not a failing case. What this file can do is pin the positive side of the same line -
 * that the types NEXT to a refused one still work - and that is what it does.
 */

#include <base/collections/dynamic_bitset.hpp>
#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/collections/stable_container.hpp>
#include <base/collections/stable_hashmap.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/misc/raw_view.hpp>
#include <base/misc/shared_view.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/bit256.hpp>
#include <base/types/floats.hpp>
#include <base/types/ints.hpp>
#include <base/types/monostate.hpp>
#include <base/types/ok_bad.hpp>

#include <ser/base/all.hpp>
#include <ser/ser.hpp>
#include <ser/std/all.hpp>
#include <tester/tester.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


/** @brief Two ids over the same integer: nothing but their names differs on the wire. */
STRONG_TYPEDEF_INT(TestKeyA, usize);
STRONG_TYPEDEF_INT(TestKeyB, usize);

/** @brief Stateless and really plain `delete`, so opting it in below is honest. */
template<class T>
struct TestPlainDeleter final {
	void del(T* ptr) { delete ptr; }
};

template<class T>
struct ser::box_deleter_is_new_delete<TestPlainDeleter<T>> {
	static constexpr bool VALUE = true;
};

/** @brief Neither: the memory belongs to an arena that ser has no way to learn about. */
template<class T>
struct TestArenaDeleter final {
	void* arena = nullptr;

	void del(T* ptr) { delete ptr; }
};

using ByteBuf = std::vector<std::byte>;

/** @brief The whole buffer as the span ser::read wants. */
std::span<const std::byte> view(const ByteBuf& b) { return { b.data(), b.size() }; }

/** @brief The first `n` bytes of it, for the truncation cases. */
std::span<const std::byte> view(const ByteBuf& b, usize n) {
	return { b.data(), n < b.size() ? n : b.size() };
}

/** @brief Bytes of one object, for the cases that check the wire size. */
template<class T>
ByteBuf bytesOf(const T& x) {
	ByteBuf buf;
	(void) ser::write(buf, x);
	return buf;
}

struct Mixed {
	base::Optional<i32>    maybe;
	base::Box<std::string> boxed;
	base::OwningView       blob;

	friend bool operator==(const Mixed& a, const Mixed& b) {
		return a.maybe == b.maybe && *a.boxed == *b.boxed
		    && a.blob.view().stringView() == b.blob.view().stringView();
	}
};

class SerBaseTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SerBaseTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(plainValues);
		TESTER_ADD_TEST(optionals);
		TESTER_ADD_TEST(owningPointers);
		TESTER_ADD_TEST(boxDeleters);
		TESTER_ADD_TEST(byteViews);
		TESTER_ADD_TEST(maps);
		TESTER_ADD_TEST(stableVectors);
		TESTER_ADD_TEST(bitsets);
		TESTER_ADD_TEST(mixedAggregate);
		TESTER_ADD_TEST(stdInterop);
		TESTER_ADD_TEST(corruptStreams);
	}

	~SerBaseTest() override = default;

private:
	/**
	 * @brief The types that need no adapter, pinned so that stays true.
	 *
	 * OkBad is an aggregate over an enum class with bool as its underlying type, so it
	 * rides the member walk and the byte is validated on the way back. Monostate is an
	 * empty aggregate and writes nothing at all - a type whose whole wire form is its
	 * absence. Bit256 needs an adapter only because it has constructors.
	 */
	void plainValues() {
		ASSERT_TRUE(roundTrip(base::OK).isOk());
		ASSERT_TRUE(roundTrip(base::BAD).isBad());
		ASSERT_EQUAL(usize{ 1 }, bytesOf(base::OK).size());

		ASSERT_TRUE(bytesOf(base::Monostate{}).empty());
		ASSERT_EQUAL(usize{ 0 }, ser::MIN_WIRE_SIZE_V<base::Monostate>);

		const base::Bit256 hash{ 1, 2, 3, 4 };
		ASSERT_TRUE(roundTrip(hash) == hash);
		ASSERT_EQUAL(usize{ 32 }, bytesOf(hash).size());
	}

	/** @brief base::Optional: one presence byte, then the value if there is one. */
	void optionals() {
		ASSERT_EQUAL(i32{ 5 }, roundTrip(base::Optional<i32>(5)).value());
		ASSERT_TRUE(!roundTrip(base::Optional<i32>()).has_value());
		ASSERT_EQUAL(usize{ 1 }, bytesOf(base::Optional<i32>()).size());
		ASSERT_EQUAL(usize{ 1 + sizeof(i32) }, bytesOf(base::Optional<i32>(5)).size());

		/*
		 * A payload that is not trivially copyable, so the value travels through its own
		 * adapter rather than as bytes.
		 */
		const auto text = roundTrip(base::Optional<std::string>("hi"));
		ASSERT_TRUE(text.has_value());
		ASSERT_EQUAL(std::string("hi"), text.value());

		/**
		 * @brief Reading an empty one over an engaged one has to clear it, not leave the old
		 * value behind.
		 */
		base::Optional<i32> target(7);
		const auto          empty = bytesOf(base::Optional<i32>());
		ser::in             ar{ view(empty) };
		ASSERT_EQUAL(ser::Errc::Ok, ar(target));
		ASSERT_TRUE(!target.has_value());
	}

	/**
	 * @brief Box is transparent, MBox is an optional.
	 *
	 * "Transparent" is the pinned property: a Box<T> writes exactly what a T writes, so
	 * the wire size is the value's and nothing marks that a pointer was involved.
	 */
	void owningPointers() {
		const auto boxed = bytesOf(base::makeBox<i32>(42));
		ASSERT_EQUAL(sizeof(i32), boxed.size());
		ASSERT_EQUAL(i32{ 42 }, *ser::readOrPanicForce<base::Box<i32>>(view(boxed)));

		base::MBox<std::string> full  = base::makeBox<std::string>("text");
		const auto              bytes = bytesOf(full);
		const auto              back  = ser::readOrPanicForce<base::MBox<std::string>>(view(bytes));
		ASSERT_TRUE(static_cast<bool>(back));
		ASSERT_EQUAL(std::string("text"), *back);

		const auto null_bytes = bytesOf(base::MBox<std::string>{});
		ASSERT_EQUAL(usize{ 1 }, null_bytes.size());
		const auto null_back = ser::readOrPanicForce<base::MBox<std::string>>(view(null_bytes));
		ASSERT_TRUE(!static_cast<bool>(null_back));

		/* Reading a null over an engaged MBox releases what it held. */
		base::MBox<std::string> target = base::makeBox<std::string>("old");
		ser::in                 ar{ view(null_bytes) };
		ASSERT_EQUAL(ser::Errc::Ok, ar(target));
		ASSERT_TRUE(!static_cast<bool>(target));
	}

	/**
	 * @brief A Box's deleter has to pair with the `new` that a read performs.
	 *
	 * Reading a Box allocates with `new` and default-constructs the deleter, so a custom
	 * deleter - an arena, a pool, malloc - is refused outright, and the refusal is a
	 * static_assert this file cannot exercise. What it CAN pin is the opt-in for a deleter
	 * that keeps both halves of the promise: stateless, and really plain `delete`.
	 */
	void boxDeleters() {
		static_assert(ser::BOX_DELETER_IS_NEW_DELETE_V<base::DefaultBoxPtrDeleter<i32>>);
		static_assert(ser::BOX_DELETER_IS_NEW_DELETE_V<TestPlainDeleter<i32>>);
		static_assert(not ser::BOX_DELETER_IS_NEW_DELETE_V<TestArenaDeleter<i32>>);

		using PlainBox  = base::Box<i32, TestPlainDeleter<i32>>;
		using PlainMBox = base::MBox<i32, TestPlainDeleter<i32>>;

		/*
		 * The deleter is not on the wire, so an opted-in Box is still transparent and still
		 * interchangeable with the default-deleter one.
		 */
		const auto bytes = bytesOf(base::makeBox<i32, TestPlainDeleter<i32>>(42));
		ASSERT_EQUAL(sizeof(i32), bytes.size());
		ASSERT_EQUAL(i32{ 42 }, *ser::readOrPanicForce<PlainBox>(view(bytes)));
		ASSERT_EQUAL(i32{ 42 }, *ser::readOrPanicForce<base::Box<i32>>(view(bytes)));
		static_assert(ser::schemaHash<PlainBox>() == ser::schemaHash<i32>());
		static_assert(ser::schemaHash<PlainMBox>() == ser::schemaHash<std::optional<i32>>());

		const auto engaged = bytesOf(PlainMBox{ base::makeBox<i32, TestPlainDeleter<i32>>(7) });
		ASSERT_EQUAL(i32{ 7 }, *ser::readOrPanicForce<PlainMBox>(view(engaged)));
		const auto null_bytes = bytesOf(PlainMBox{});
		ASSERT_TRUE(!static_cast<bool>(ser::readOrPanicForce<PlainMBox>(view(null_bytes))));
	}

	/** @brief The owning views: a length prefix and that many bytes, copied in one go. */
	void byteViews() {
		const auto owning = bytesOf(base::OwningView::copy(base::RawView("hello")));
		ASSERT_EQUAL(sizeof(u64) + 5, owning.size());
		const auto owning_back = ser::readOrPanicForce<base::OwningView>(view(owning));
		ASSERT_EQUAL(std::string_view("hello", 5), owning_back.view().stringView());

		const auto shared      = bytesOf(base::SharedView::copy(base::RawView("shared")));
		const auto shared_back = ser::readOrPanicForce<base::SharedView>(view(shared));
		ASSERT_EQUAL(std::string_view("shared", 6), shared_back.view().stringView());

		/*
		 * Empty is a length and nothing else, and it has to survive as empty rather than
		 * as a view onto one byte.
		 */
		const auto empty = bytesOf(base::OwningView{});
		ASSERT_EQUAL(sizeof(u64), empty.size());
		ASSERT_EQUAL(usize{ 0 }, ser::readOrPanicForce<base::OwningView>(view(empty)).view().size());
	}

	/**
	 * @brief The four map shapes.
	 *
	 * VectorMap and StableHashMap are read in place because neither is movable in the way
	 * ser::read needs; that is the supported shape for them and this pins it.
	 */
	void maps() {
		base::Map<std::string, i32> ordered;
		ordered.put("a", 1);
		ordered.put("b", 2);
		const auto ordered_back = roundTrip(ordered);
		ASSERT_EQUAL(usize{ 2 }, ordered_back.size());
		ASSERT_EQUAL(i32{ 1 }, ordered_back["a"]);
		ASSERT_EQUAL(i32{ 2 }, ordered_back["b"]);

		base::HashMap<i32, i32> hashed;
		hashed.put(3, 4);
		ASSERT_EQUAL(i32{ 4 }, roundTrip(hashed)[3]);

		/*
		 * A hole in the middle and a used slot after it: the dense format is what keeps
		 * the indices meaning the same thing on the other side.
		 */
		base::VectorMap<usize, i32> vec_map;
		vec_map.put(0, 10);
		vec_map.put(3, 13);
		base::VectorMap<usize, i32> vec_back;
		readInPlace(bytesOf(vec_map), vec_back);
		ASSERT_EQUAL(usize{ 2 }, vec_back.size());
		ASSERT_EQUAL(i32{ 10 }, vec_back[0]);
		ASSERT_EQUAL(i32{ 13 }, vec_back[3]);
		ASSERT_TRUE(!vec_back.contains(1));

		/*
		 * element_count is recomputed rather than read, so the count and the slots cannot
		 * disagree: nothing on the wire says how many there are.
		 */
		ASSERT_EQUAL(bytesOf(vec_map).size(), bytesOf(vec_back).size());

		base::StableHashMap<i32, std::string> stable;
		stable.put(1, "one");
		stable.put(2, "two");
		base::StableHashMap<i32, std::string> stable_back;
		readInPlace(bytesOf(stable), stable_back);
		ASSERT_EQUAL(usize{ 2 }, stable_back.size());
		ASSERT_EQUAL(std::string("one"), *stable_back.atMaybe(1).value());
		ASSERT_EQUAL(std::string("two"), *stable_back.atMaybe(2).value());
	}

	/**
	 * @brief StableVector, in both the mutable and the locked form.
	 *
	 * The locked form - what toConstData() hands out - takes the same format through the
	 * same adapter: the const is about what the accessors return, not about the container,
	 * so it is still filled by appending.
	 *
	 * The target has to be EMPTY. Unlike std::vector, this container promises that a Ref
	 * handed out stays valid, so the adapter cannot quietly clear it - it refuses instead.
	 * ser::read default-constructs its target, so the only way to hit that is to read into
	 * a container you already filled.
	 */
	void stableVectors() {
		base::StableVector<i32> numbers;
		numbers.pushBack(1);
		numbers.pushBack(2);
		numbers.pushBack(3);
		const auto bytes = bytesOf(numbers);

		base::StableVector<i32> back;
		readInPlace(bytes, back);
		ASSERT_EQUAL(usize{ 3 }, back.size());
		ASSERT_EQUAL(i32{ 1 }, *back[0]);
		ASSERT_EQUAL(i32{ 3 }, *back[2]);

		/*
		 * A Ref taken before the read is still the element it was: nothing moved, and
		 * nothing was destroyed under it.
		 */
		const auto first = back[0];
		ASSERT_EQUAL(i32{ 1 }, *first);

		base::StableVector<const i32> locked;
		readInPlace(bytes, locked);
		ASSERT_EQUAL(usize{ 3 }, locked.size());
		ASSERT_EQUAL(i32{ 2 }, *locked[1]);

		/* Same bytes either way, which is what makes one schema honest for both. */
		base::StableVector<i32> other;
		other.pushBack(1);
		other.pushBack(2);
		other.pushBack(3);
		ASSERT_EQUAL(bytes.size(), bytesOf(std::move(other).toConstData()).size());
	}

	/**
	 * @brief DynamicBitset: the capacity, then the words.
	 *
	 * The capacity is on the wire because it cannot be recovered from the words - three
	 * bits and sixty-four both occupy one word - and the type is not resizable, so its
	 * size is data.
	 */
	void bitsets() {
		base::DynamicBitset bits(70);
		bits.set(0);
		bits.set(69);

		const auto bytes = bytesOf(bits);
		ASSERT_EQUAL(sizeof(u64) + 2 * sizeof(u64), bytes.size());

		const auto back = ser::readOrPanicForce<base::DynamicBitset>(view(bytes));
		ASSERT_EQUAL(usize{ 70 }, back.size());
		ASSERT_EQUAL(usize{ 2 }, back.count());
		ASSERT_TRUE(back.test(0));
		ASSERT_TRUE(back.test(69));
		ASSERT_TRUE(!back.test(1));

		/* An empty one is a capacity of zero and no words - not one word of zeroes. */
		const auto empty = bytesOf(base::DynamicBitset{});
		ASSERT_EQUAL(sizeof(u64), empty.size());
		ASSERT_EQUAL(usize{ 0 }, ser::readOrPanicForce<base::DynamicBitset>(view(empty)).size());
	}

	/** @brief One aggregate holding three base types with three different read paths. */
	// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks): the analyzer cannot follow
	/*
	 * Box's deleter and reports every Box going out of scope as a leak - the same false
	 * positive box.hpp already suppresses, see
	 * https://github.com/ducktype-org/duckling/issues/402
	 */
	void mixedAggregate() {
		const Mixed sample{ .maybe = base::Optional<i32>(11),
			                .boxed = base::makeBox<std::string>("inside"),
			                .blob  = base::OwningView::copy(base::RawView("bytes")) };

		const auto bytes = bytesOf(sample);
		const auto back  = ser::readOrPanicForce<Mixed>(view(bytes));
		assertTrue(back == sample, "the mixed aggregate did not survive the round trip");
	}

	// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

	/**
	 * @brief The streams that are deliberately interchangeable with their std form.
	 *
	 * Each of these pairs writes identical bytes, and the schema hashes say so - which is
	 * what lets a payload change which container it uses without changing its format.
	 */
	void stdInterop() {
		static_assert(
			ser::schemaHash<base::Map<i32, i32>>() == ser::schemaHash<std::map<i32, i32>>()
		);
		static_assert(
			ser::schemaHash<base::StableHashMap<i32, i32>>() == ser::schemaHash<std::map<i32, i32>>()
		);
		static_assert(
			ser::schemaHash<base::Optional<u32>>() == ser::schemaHash<std::optional<u32>>()
		);
		static_assert(ser::schemaHash<base::Box<i32>>() == ser::schemaHash<i32>());
		static_assert(ser::schemaHash<base::MBox<i32>>() == ser::schemaHash<std::optional<i32>>());
		static_assert(
			ser::schemaHash<base::StableVector<i32>>() == ser::schemaHash<std::vector<i32>>()
		);
		static_assert(ser::schemaHash<base::OwningView>() == ser::schemaHash<base::SharedView>());

		/*
		 * And the same distinctions the std adapters make are still made here, so the
		 * envelope can still refuse the wrong type.
		 */
		static_assert(
			ser::schemaHash<base::Map<i32, i32>>() != ser::schemaHash<base::Map<i32, f32>>()
		);
		static_assert(
			ser::schemaHash<base::Optional<u32>>() != ser::schemaHash<base::Optional<f32>>()
		);
		static_assert(
			ser::schemaHash<base::VectorMap<usize, i32>>()
			!= ser::schemaHash<base::VectorMap<usize, f32>>()
		);

		/*
		 * The KEY of a VectorMap is never on the wire - it is the index - so two maps keyed
		 * by different id types write byte-identical streams, and the hash is the only thing
		 * that can tell them apart. That is what serializer<VectorMap>'s schema mixes the key
		 * for, and it only works because a strong typedef says what it wraps: without that
		 * the two keys below would both hash as sizeof + alignof and come out equal.
		 */
		static_assert(
			ser::schemaHash<base::VectorMap<TestKeyA, i32>>()
			!= ser::schemaHash<base::VectorMap<TestKeyB, i32>>()
		);
		static_assert(ser::schemaHash<TestKeyA>() != ser::schemaHash<usize>());

		/*
		 * The bytes, not only the hashes: a base::Map stream really does read into a
		 * std::map, and a std::vector stream into a StableVector.
		 */
		base::Map<std::string, i32> ordered;
		ordered.put("k", 5);
		const auto as_std
			= ser::readOrPanicForce<std::map<std::string, i32>>(view(bytesOf(ordered)));
		ASSERT_EQUAL(usize{ 1 }, as_std.size());
		ASSERT_EQUAL(i32{ 5 }, as_std.at("k"));

		const std::vector<i32>  plain{ 4, 5, 6 };
		base::StableVector<i32> stable;
		readInPlace(bytesOf(plain), stable);
		ASSERT_EQUAL(usize{ 3 }, stable.size());
		ASSERT_EQUAL(i32{ 6 }, *stable[2]);

		/**
		 * @brief The envelope refuses a type whose schema differs, which is the whole point of
		 * the specializations above being specific.
		 */
		ser::options opt{};
		opt.header = true;
		ByteBuf with_header;
		ASSERT_TRUE(ser::write(with_header, base::Optional<u32>(1U), opt).has_value());
		ASSERT_TRUE(!ser::read<base::Optional<f32>>(view(with_header), opt).has_value());
		ASSERT_TRUE(ser::read<std::optional<u32>>(view(with_header), opt).has_value());
	}

	/**
	 * @brief What a damaged or hostile stream gets: an error code, at the right place.
	 *
	 * These are the cases where an adapter could plausibly do something worse than fail -
	 * allocate on a bogus length, load a bitset that contradicts its own capacity, or take
	 * the second of two identical keys - so each is pinned to the code it must return.
	 */
	void corruptStreams() {
		/**
		 * @brief A bit set above the capacity. Three bits means one word, and bit five of that
		 * word is outside the object: loading it would leave count() disagreeing with
		 * size() for the rest of the object's life.
		 */
		base::DynamicBitset small(3);
		small.set(1);
		ByteBuf tampered = bytesOf(small);
		ASSERT_EQUAL(sizeof(u64) + sizeof(u64), tampered.size());
		tampered[sizeof(u64)] |= std::byte{ 0x20 };
		base::DynamicBitset target;
		ser::in             tampered_ar{ view(tampered) };
		ASSERT_EQUAL(ser::Errc::InvalidValue, tampered_ar(target));

		/**
		 * @brief A capacity nobody could have written: the words it implies are not in the stream,
		 * so it is refused before any allocation rather than after. The bound is the stream
		 * itself and not a policy ceiling - a bitset that really is huge still loads.
		 */
		ByteBuf  absurd;
		ser::out absurd_ar{ absurd };
		ASSERT_EQUAL(ser::Errc::Ok, absurd_ar(u64{ 1 } << 40));
		ser::in absurd_read{ view(absurd) };
		ASSERT_EQUAL(ser::Errc::Truncated, absurd_read(target));

		/* The prefix is there and the words are not. */
		const auto truncated = bytesOf(small);
		ser::in    truncated_ar{ view(truncated, sizeof(u64)) };
		ASSERT_EQUAL(ser::Errc::Truncated, truncated_ar(target));

		/**
		 * @brief The same key twice in a map stream. put() would panic on it, so the adapter has
		 * to use the checking insert and report corrupt input instead.
		 */
		ByteBuf  repeated;
		ser::out repeated_ar{ repeated };
		ASSERT_EQUAL(
			ser::Errc::Ok,
			repeated_ar(u64{ 2 }, i32{ 1 }, std::string("a"), i32{ 1 }, std::string("b"))
		);
		base::StableHashMap<i32, std::string> stable;
		ser::in                               repeated_read{ view(repeated) };
		ASSERT_EQUAL(ser::Errc::InvalidValue, repeated_read(stable));

		/** @brief A view whose length prefix promises more bytes than the stream holds. */
		ByteBuf  lying;
		ser::out lying_ar{ lying };
		ASSERT_EQUAL(ser::Errc::Ok, lying_ar(u64{ 64 }, u32{ 0 }));
		base::OwningView blob;
		ser::in          lying_read{ view(lying) };
		ASSERT_EQUAL(ser::Errc::Truncated, lying_read(blob));
	}

	/** @brief Writes `sample` and reads it back by value, for the types that can be. */
	template<class T>
	static T roundTrip(const T& sample) {
		const auto bytes = bytesOf(sample);
		return ser::readOrPanicForce<T>(view(bytes));
	}

	/** @brief Reads into an existing object, for the types that cannot be returned. */
	template<class T>
	void readInPlace(const ByteBuf& bytes, T& target) {
		ser::in ar{ view(bytes) };
		ASSERT_EQUAL(ser::Errc::Ok, ar(target));
	}
};

TESTER_COMMON_MAIN("/src/common/ser/tests/");
