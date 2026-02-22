#include <base/memory/manual_lifetime_storage.hpp>
#include <base/misc/noexcept.hpp>
#include <base/pointers/box.hpp>

namespace base {

    // note BYTE_BLOCK_SIZE is here in only for compatibility with SingleTypeMemoryPoolAllocator, it is not used in StdAllocator
	template<class T, u64 BYTE_BLOCK_SIZE = 4 * 1'024>
	requires base::IsPlainType<T> class StdAllocator final {
    public:
		Ref<T> allocateEmplace(auto&&... args) {
            return new T(std::forward<decltype(args)>(args)...);
        }

		void justDestroy(Ref<T> obj_ref) {
			delete obj_ref.get();
		}

		void deallocateDestroy(Ref<T> obj_ref) {
			delete obj_ref.get();
		}
	};
}
