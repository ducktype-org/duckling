#pragma once

constexpr int NUM_THREADS = 1;


struct THide final {
	long long x;
	long long y;
	long long z;
	long long w;
};

THide* allocateHide();
