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
