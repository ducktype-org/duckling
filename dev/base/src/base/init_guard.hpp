#pragma once

#define RIFT_SIMPLE_INIT_GUARD_BEGIN        \
	static bool _detail_was_init = false;   \
	if (_detail_was_init) {                 \
		return;                             \
	}

#define RIFT_SIMPLE_INIT_GUARD_END      \
	_detail_was_init = true; 
