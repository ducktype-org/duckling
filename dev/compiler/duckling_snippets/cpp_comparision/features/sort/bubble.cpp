#include <vector>

template<typename T>
void sort(std::vector<T> &data) {
    bool swapped = false;
    for (int i=0; i<data.size(); i++) {
        swapped = false;
        for (int j = 1; j < data.size(); j++) {
            if (data[j] < data[j-1]) {
                std::swap(data[j], data[j-1]);
                swapped = true;
            }
        }
        if (!swapped) break;
    }
}
