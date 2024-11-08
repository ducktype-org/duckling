#include <iostream>
#include <vector>

template<typename T>
void sort(std::vector<T> &data) {
    for (auto i = data.begin(); i != data.end(); ++i) {
        auto min = i;
        for (auto j = i; j != data.end(); ++j) {
            if (*j < *min) min = j;
        }
        std::swap(*i, *min);
    }
}

int main() {
    int n;
    std::vector<int> data;

    std::cin >> n;
    for (int i = 0; i < n; i++) {
        int tmp;
        std::cin >> tmp;
        data.push_back(tmp);
    }

    sort(data);
    
    for (int i = 0; i < n; i++) {
        std::cout << data[i] << " ";
    }
    std::cout << "\n";
}
