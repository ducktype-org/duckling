#include <vector>

template<typename T>
void sort(std::vector<T> &data) {
    for (int i=1; i<data.size(); i++) {
        T current = data[i];
        int j=i-1;
        while (j >= 0 && data[j] > current) {
            data[j+1] = data[j];
            j--;
        }
        data[j+1] = current;
    }
}
