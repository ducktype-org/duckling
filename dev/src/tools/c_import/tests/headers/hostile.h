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
