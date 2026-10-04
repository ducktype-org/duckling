/**
 * Note that this file does not include some dependencies
 * to avoid heavy dependencies on the query framework (i.e. entire compiler).
 * User of the macros should include necessary headers.
 */

#pragma once

/**
 * Add a static function to Query Implementation Struct that
 * returns given query artifact collection.
 */
#define QUERY_ARTIFACTS_MACROS                                                                  \
	static Ref<artifacts::ArtifactCollection> getQueryArtifactsCollection() {                   \
		static Ref<artifacts::ArtifactCollection> collection                                    \
			= global_state::getRootCollection()                                                 \
		          ->subCollectionAtOrNew(base::StrID("query"))                                  \
		          ->subCollectionAtOrNew(                                                       \
					  base::StrID(base::strConcat("query", QueryType::getID().asInt()).c_str()) \
				  );                                                                            \
		return collection;                                                                      \
	}
