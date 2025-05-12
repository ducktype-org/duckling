#include "artman.hpp"

#include <unordered_map>

void artman::BlobArtifact::setData(const byte* ptr, usize n_bytes) {
	PARENT->setBlobData(*this, ptr, n_bytes);
}
