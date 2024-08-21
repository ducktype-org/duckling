#include <base/smart_pointers.hpp>

// we know we don't own `borrowed`:
void borrows(base::borrow_ptr<int> borrowed) { (*borrowed)++; }

int main() {
	base::unique_ptr<int> i = base::make_unique<int>(0);

	// no need fot .get():
	borrows(i.borrow_mut());
}
