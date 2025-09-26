#pragma once

#include <concurrent/worker.hpp>

#include <base/ref.hpp>

constexpr int NUM_THREADS = 8;

struct THide final {
	long long x;
	long long y;
	long long z;
	long long w;
};

THide* allocateHide(Ref<concurrent::WorkerData>);
