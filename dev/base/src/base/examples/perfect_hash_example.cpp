#include <base/perfect_hash.hpp>

struct T {
	base::HashT customPerfectHash() { ... }
}

struct Q {
} base::HashT customPerfectHash(Q) { ... }

int main() {
	base::perfectHash(T());
	base::perfectHash(Q());
}
