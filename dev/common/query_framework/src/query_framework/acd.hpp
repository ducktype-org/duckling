#pragma once

/**
 * @brief Additional Cache data.
 * This is a struct that needs to be stored along side every "cache entry".
 */
struct ACD {
	// ...
};

template <typename Data>
struct AddACD {
	Data data;
	ACD acd;
};
