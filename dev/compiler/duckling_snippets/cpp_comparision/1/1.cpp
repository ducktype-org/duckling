#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdint>

using u64 = uint64_t;
using i64 = int64_t;
struct Vec {
	i64 x, y;

	bool operator<(const Vec& oth) const {
		if (x != oth.x) return x < oth.x;
		return y < oth.y;
	};
};

enum class Direction {
	PG,
	PD,
	LG,
	LD
};

struct DivVec {
	Vec position;
	Direction dir;
	
	bool operator<(const DivVec& oth) const {
		if (position < oth.position) return true;
		if (oth.position < position) return false;
		return dir < oth.dir;
	}
};

struct Flips {
	bool x_flip = false;
	bool y_flip = false;
};

struct MirrorPoint {
	Vec position;
	Flips flips;
};

std::vector<MirrorPoint> mirrors;

i64 sign(i64 x) {
	return x > 0 ? 1 : (x < 0 ? (-1) : 0);
}

Vec dirToVec(Direction d) {
	switch (d) {
	case Direction::PG:
		return {1, 1};
	case Direction::PD:
		return {1, -1};
	case Direction::LG:
		return {-1, 1};
	case Direction::LD:
		return {-1, -1};
	}
	throw "aaa";
}

Direction getDirection(std::string_view s) {
	if (s == "PG") return Direction::PG;
	if (s == "PD") return Direction::PD;
	if (s == "LG") return Direction::LG;
	if (s == "LD") return Direction::LD;
	throw "hmmm";
}

// PG - x no flip, y no flip
// PD - x no flip, y flip
// LG - x flip,    y no flip
// LD - x flip,    y flip

i64 diagID(bool flip_x, bool flip_y, Vec v) {
	return (flip_x ? (v.x) : (-v.x)) - (flip_y ? (v.y) : (-v.y));
}

auto compareVecOnDiagonal(bool flip_x, [[maybe_unused]] bool flip_y) {
	return [=](MirrorPoint lhs, MirrorPoint rhs) {
		if (flip_x) {
			return lhs.position.x > rhs.position.x;
		}
		else {
			return lhs.position.x < rhs.position.x;
		}
	};

}

Direction flipDirection(Direction d, MirrorPoint mirror) {
	auto [x_flip, y_flip] = mirror.flips;
	auto [x, y] = dirToVec(d);

	if (x_flip) x *= -1;
	if (y_flip) y *= -1;

	if (x == 1 and y == 1) return Direction::PG;
	if (x == 1 and y == -1) return Direction::PD;
	if (x == -1 and y == 1) return Direction::LG;
	if (x == -1 and y == -1) return Direction::LD;

	throw "hmm";
}

std::map<DivVec, DivVec> from_to;

void makeDiag(Direction diag_direction) {
	// this are different flips:
	auto flip_x = diag_direction == Direction::LG or diag_direction == Direction::LD;
	auto flip_y = diag_direction == Direction::PD or diag_direction == Direction::LD;

	std::map<i64, std::vector<MirrorPoint> > mirror_per_diag;

	for (auto mirror : mirrors) {
		auto diag_id = diagID(flip_x, flip_y, mirror.position);
		mirror_per_diag[diag_id].push_back(mirror);
	}
		
	for (auto& [_, mirrors_on_diag]: mirror_per_diag) {
		std::sort(mirrors_on_diag.begin(), mirrors_on_diag.end(), compareVecOnDiagonal(flip_x, flip_y));

		for (i64 i = 0; i < std::ssize(mirrors_on_diag) - 1; i++) {
			auto from = mirrors_on_diag.at(i);
			auto to = mirrors_on_diag.at(i + 1);

			from_to.insert({{from.position, diag_direction}, {to.position, flipDirection(diag_direction, to)}});
			
		}
	}
}



// int main() {
// 	std::ios_base::sync_with_stdio(0);
// 	std::cin.tie(0);
	
// 	i64 box_size;

// 	u64 mirror_count;
// 	std::cin >> mirror_count;

// 	std::cin >> box_size;

// 	std::map<Vec, Flips> mirror_points_mapped;

// 	std::vector<std::tuple<i64, i64, i64, i64>> input;
// 	for (u64 i = 0; i < mirror_count; i++) {
// 		i64 xb, yb, xe, ye;
// 		std::cin >> xb >> yb >> xe >> ye;
// 		input.emplace_back(xb, yb, xe, ye);
// 	}

// 	input.push_back({0, 0, box_size, 0});
// 	input.push_back({0, 0, 0, box_size});
// 	input.push_back({box_size, 0, box_size, box_size});
// 	input.push_back({0, box_size, box_size, box_size});

// 	for (auto [xb, yb, xe, ye]: input) {
// 		// i64 xb, yb, xe, ye;
// 		// std::cin >> xb >> yb >> xe >> ye;

// 		i64 xd = sign(xe - xb);
// 		i64 yd = sign(ye - yb);
		
// 		i64 x = xb;
// 		i64 y = yb;

// 		bool x_flip = yd != 0;
// 		bool y_flip = xd != 0;
		
// 		bool go = true;
// 		while (go) {
// 			if (x == xe and y == ye)
// 				go = false;

// 			mirror_points_mapped[{x, y}].x_flip |= x_flip;
// 			mirror_points_mapped[{x, y}].y_flip |= y_flip;

// 			x += xd;
// 			y += yd;
			
// 		}
// 	}
	
// 	for (auto [pos, flips]: mirror_points_mapped) {
// 		mirrors.emplace_back(MirrorPoint(pos, flips));
// 	}

// 	makeDiag(Direction::PG);
// 	makeDiag(Direction::PD);
// 	makeDiag(Direction::LG);
// 	makeDiag(Direction::LD);

// 	u64 q;
// 	std::cin >> q;

// 	std::map<DivVec, Vec> cache;

// 	for (u64 i = 0; i < q; i++) {
// 		Vec start(0, 0);
// 		std::cin >> start.x >> start.y;

// 		std::string s;
// 		std::cin >> s;

// 		auto direction = getDirection(s);

// 		DivVec start_position(start, direction);
// 		DivVec current = start_position;


// 		if (cache.contains(current)) {
// 			std::cout << cache.at(current).x << " " << cache.at(current).y << "\n";
// 			continue;
// 		}

// 		while (true) {
// 			current = from_to.at(current);

// 			// std::cerr << "bouce: ";
// 			// std::cerr << current.position.x << " " << current.position.y << " " << (int)current.dir << "\n";

// 			if (current.position.x == 0 or current.position.y == 0 or current.position.x == box_size or current.position.y == box_size) {
// 				break;
// 			}

// 		}

// 		cache.insert({start_position, current.position});

// 		std::cout << current.position.x << " " << current.position.y << "\n";
// 	}

// }
