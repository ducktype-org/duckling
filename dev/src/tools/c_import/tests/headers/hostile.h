union hostile_union {
	int    i;
	double d;
};

struct hostile_bits {
	unsigned a: 3;
	unsigned b: 5;
};

struct hostile_flexible {
	int count;
	int values[];
};

struct hostile_has_union {
	int                 tag;
	union hostile_union payload;
};

int hostile_printf(const char* fmt, ...);

static inline int hostile_inline(int a) { return a; }

int   hostile_callback(int (*compare)(int, int));
void* hostile_void_pointer(void* p);

__int128    hostile_int128(__int128 a);
long double hostile_long_double(long double a);

/* `match` is a Duckling keyword, so this cannot be declared under its own name. */
int match(int a);

/* `type` is a Duckling keyword, so the parameter gets a positional name instead. */
int hostile_keyword_param(int type);

/* libclang reports an anonymous member as a nested record, not a field. Dropping it would
   silently change the record's size and offsets, so the whole record has to go. */
struct hostile_anon_member {
	int tag;
	union {
		int   i;
		float f;
	};
};

/* A nested definition is not a top-level cursor, so it has to be discovered separately. */
struct hostile_outer {
	struct hostile_inner {
		int a;
	} inner;
};

/* An unnamed record used as a named field still needs a class of its own. */
struct hostile_named_anon {
	struct {
		int p;
		int q;
	} pos;
};

/* `holder` is declared before `later` is known to be unsupported. */
struct hostile_later;

struct hostile_holder {
	struct hostile_later* l;
	int                   x;
};

struct hostile_later {
	int bits: 3;
};

/* `type` is a Duckling keyword, but a field name is positional in the C ABI and never linked,
   so it can be renamed instead of costing the whole record. */
struct hostile_keyword_field {
	int type;
	int normal;
};

/* Duckling has no way to spell either of these, and emitting the natural layout would place
   every following field wrong with nothing to catch it. */
struct __attribute__((packed)) hostile_packed {
	char a;
	int  b;
};

struct __attribute__((aligned(16))) hostile_overaligned {
	int a;
};
