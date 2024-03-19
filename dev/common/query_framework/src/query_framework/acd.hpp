#pragma once


/**
 * @brief Additional Cache data
 */
struct ACD {
	// ...
};

template <typename Data>
struct AddACD {
	Data data;
	ACD acd;
};
