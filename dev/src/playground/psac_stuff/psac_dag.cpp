#include <psac/psac.hpp>

#include <base/types/ints.hpp>

#include <vector>


usize                              g_n;
u64                                g_mod;
std::vector<u64>                   g_weights;
std::vector<std::vector<usize>>    g_adj;
std::vector<std::pair<usize, u64>> g_updates;

// from mapreduce example
template<typename It>
psac_function(sum, const u64 mod, It in_begin, It in_end, psac::Mod<usize>* result) {
	if (in_begin + 1 == in_end) {
		psac_read((auto x), (*in_begin), { psac_write(result, x); });
	} else {
		auto              in_mid       = in_begin + std::distance(in_begin, in_end) / 2;
		psac::Mod<usize>* left_result  = psac_alloc(usize);
		psac::Mod<usize>* right_result = psac_alloc(usize);
		psac_par(
			psac_call(sum, mod, in_begin, in_mid, left_result),
			psac_call(sum, mod, in_mid, in_end, right_result)
		);
		psac_read((auto x, auto y), (left_result, right_result), {
			psac_write(result, (x + y) % mod);
		});
	}
}

psac_function(
	compute,
	usize                                  n,
	u64                                    mod,
	std::vector<psac::Mod<u64>>*           w,
	const std::vector<std::vector<usize>>& e,
	std::vector<psac::Mod<u64>>*           out
) {
	for (usize i = 0; i < n; ++i) {
		std::vector<psac::Mod<usize>*> in_e;
		in_e.push_back(&(*w)[i]);
		for (usize j: e[i]) in_e.push_back(&(*out)[static_cast<usize>(j)]);
		psac_call(sum, mod, in_e.begin(), in_e.end(), &(*out)[i]);
	}
}

int readData() {
	usize n{ 0 };
	u64   mod{ 0 };
	if (!(std::cin >> n >> mod)) return 1;
	g_mod = mod;
	g_weights.resize(n);
	for (usize i = 0; i < n; ++i) std::cin >> g_weights[i];

	usize m{ 0 };
	std::cin >> m;
	g_adj.resize(n);
	for (int i = 0; i < m; ++i) {
		usize u{ 0 };
		usize v{ 0 };
		std::cin >> u >> v;
		g_adj[v].push_back(u);
	}

	usize c = 0;
	std::cin >> c;
	std::vector<std::pair<int, u64>> updates(c);
	for (usize i = 0; i < c; ++i) {
		usize x = 0;
		u64   y = 0;
		std::cin >> x >> y;
		g_updates.emplace_back(x, y);
	}
	return 0;
}

int main(int argc, const char* argv[]) {
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);

	if (argc == 1) {
		std::cout << "Usage: " << argv[0] << " [\"init\"/\"calc\"/\"update\"]\n";
		return 1;
	}
	std::string user_value = argv[1];
	if (user_value != "init" && user_value != "calc" && user_value != "update") {
		std::cout << "Usage: " << argv[0] << " [\"init\"/\"calc\"/\"update\"]\n";
		return 1;
	}

	if (readData() != 0) return -1;
	if (user_value == "init") return 0;

	std::vector<psac::Mod<u64>> wmod(g_n);
	for (usize i = 0; i < g_n; ++i) psac_write(&wmod[i], g_weights[i]);

	std::vector<psac::Mod<u64>> res(g_n);
	auto                        comp = psac_run(compute, g_n, g_mod, &wmod, g_adj, &res);

	for (usize i = 0; i < g_n; ++i) std::cout << "Node " << i << " sum = " << res[i].value << "\n";

	if (user_value == "calc") return 0;

	if (user_value == "update") {
		std::cout << "UPDATE\n";

		for (const auto& [idx, new_val]: g_updates) psac_write(&wmod[idx], new_val);

		psac_propagate(comp);

		for (usize i = 0; i < g_n; ++i)
			std::cout << "Node " << i << " sum = " << res[i].value << "\n";
	}

	psac::GarbageCollector::run();
	return 0;
}
