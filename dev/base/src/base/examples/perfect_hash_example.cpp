#include <base/perfect_hash.hpp>

struct T {
	u64 a;
	/* ... */
	[[nodiscard]]
	base::HashT customPerfectHash() const { 
		return a;
	}
};

struct Q {
	u64 a; 
	/* ... */
};

base::HashT customPerfectHash(const Q& key) { 
	return key.a;
}

int main() {
	base::perfectHash(T());
	base::perfectHash(Q());
}
