#pragma once

#include <query_framework/query_int.hpp>

/**
 * Query Key.
 * note that is does not have to declared here.
 */
struct Key {
	/*...*/
};

/**
 * Query Value.
 * note that is does not have to declared here.
 */
struct Value {
	/*...*/
};

/**
 * Query declaration.
 * Under the hood it will create a struct called `MyQuery`. 
 */
DECLARE_QUERY (MyQuery, Key, Value)

