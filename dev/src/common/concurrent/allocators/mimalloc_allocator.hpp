#include <mimalloc.h>

namespace concurrent {


    // template<class T>
	// requires base::IsPlainType<T> class SingleTypeNewDeleteAllocator final {
	// public:
	// 	Ref<T> allocateEmplace(auto&&... args) {
	// 		return ::new T(std::forward<decltype(args)>(args)...);
	// 	}

	// 	void deallocateDestroy(Ref<T> obj_ref) { ::delete obj_ref.get(); }

	// 	void justDestroy(Ref<T> obj_ref) { ::delete obj_ref.get(); }
	// };
}