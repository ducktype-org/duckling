#include <vector>
#include <string>
#include <iostream>

class HeavyData {
	std::string name;
	std::vector<int> data;
public:
	HeavyData() : name("default") {}

	HeavyData(std::string name, std::vector<int> data):
		name(std::move(name)), data(std::move(data)) {}

	HeavyData(const HeavyData& other) noexcept = default;
	HeavyData(HeavyData&& other) noexcept = default;

	auto size() const {
		return data.size();
	}

	auto getName() const {
		return name;
	}

	auto accessData(size_t pos) const {
		return data[pos];
	}
};

// int main() {
// 	HeavyData data("data", {1, 2, 3, 4, 5});

// 	HeavyData moved_data(std::move(data));

// 	// Accessing moved_data
// 	std::cout << "Name: " << moved_data.getName() << "\n";
// 	std::cout << "Size: " << moved_data.size() << "\n";
// 	std::cout << "Data: ";
// 	for (size_t i = 0; i < moved_data.size(); i++) {
// 		std::cout << moved_data.accessData(i) << " ";
// 	}

// 	return 0;
// }

