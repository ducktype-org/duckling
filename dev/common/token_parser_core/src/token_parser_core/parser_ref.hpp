#pragma once

#include <base/smart_pointers.hpp>

namespace tpc {
	template<typename T>
	using ParserRef = base::unique_ptr<T>;

	template<typename T>
	using ParserBorrowRef = base::borrow_ptr<T>;

	template<typename T>
	using ParserCBorrowRef = base::c_borrow_ptr<T>;

	// this is an analogy of deduction guide for alias CTAD
	template<typename T>
	inline ParserRef<T> makeRef(T* ptr) {
		return ParserRef<T>(ptr);
	}

	// @TODO: this function does slightly different thing than
	// the function above, so maybe change its name.
	template<class T, class... Args>
	ParserRef<T> makeRef(Args&&... args) {
		return ParserRef<T>(new T(std::forward<Args>(args)...));
	}

}
