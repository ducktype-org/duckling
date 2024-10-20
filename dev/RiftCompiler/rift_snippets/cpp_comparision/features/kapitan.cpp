#include <set>
#include <iostream>
#include <vector>
#include <queue>
// using namespace std;

using std::vector;
using std::pair;
using std::set;
using std::priority_queue;
using std::cin;
using std::cout;
using std::min;
using std::abs;

int main() {
    // std::ios_base::sync_with_stdio(false);
    // std::cin.tie(nullptr);

    int n;
    set<pair<pair<int64_t, int64_t>, int>> s1;
    set<pair<pair<int64_t, int64_t>, int>> s2;
    int64_t x, y;
    cin >> n;
    int64_t ODW[n+7];
    vector<pair<int, int64_t>> Neigh[n+7];
    for (int i = 0; i < n; i++) {
        cin >> x >> y;
        s1.insert({{x, y}, i});
        s2.insert({{y, x}, i});
    }
    ODW[0] = 0;
    for (int i = 1; i < n+5; i++) 
        ODW[i] = -1;
    pair<pair<int64_t, int64_t>, int> wyspa1 = *(s1.begin());
    s1.erase(s1.begin());
    while (!s1.empty()) {
        pair<pair<int64_t, int64_t>, int> wyspa2 = *(s1.begin());
        s1.erase(s1.begin());
        int64_t dist = min(abs(wyspa1.first.first - wyspa2.first.first), abs(wyspa1.first.second - wyspa2.first.second));
        Neigh[wyspa1.second].push_back({wyspa2.second, dist});
        Neigh[wyspa2.second].push_back({wyspa1.second, dist});
        wyspa1 = wyspa2;
    }
    wyspa1 = *(s2.begin());
    s2.erase(s2.begin());
    while (!s2.empty()) {
        pair<pair<int64_t, int64_t>, int> wyspa2 = *(s2.begin());
        s2.erase(s2.begin());
        int64_t dist = min(abs(wyspa1.first.first - wyspa2.first.first), abs(wyspa1.first.second - wyspa2.first.second));
        Neigh[wyspa1.second].push_back({wyspa2.second, dist});
        Neigh[wyspa2.second].push_back({wyspa1.second, dist});
        wyspa1 = wyspa2;
    }
    priority_queue<pair<int64_t, int>> q;
    for (int i = 0; i < Neigh[0].size(); i++) {
        int wyspa = Neigh[0][i].first;
        ODW[wyspa] = Neigh[0][i].second;
        q.push({-ODW[wyspa], wyspa});
    }
    while(!q.empty()) {
        int wyspa = q.top().second;
        q.pop();
        for (int i = 0; i < Neigh[wyspa].size(); i++) {
            int cel = Neigh[wyspa][i].first;
            int64_t sciezka = Neigh[wyspa][i].second;
            if (ODW[cel] > sciezka + ODW[wyspa] || ODW[cel] == -1) {
                ODW[cel] = ODW[wyspa] + sciezka;
                q.push({-ODW[cel], cel});
                if (cel == n-1 && ODW[cel] == 0) {
                    cout << 0 << '\n';
                    return 0;
                }
            }
        }
    }
    cout << ODW[n-1] << '\n';
}
