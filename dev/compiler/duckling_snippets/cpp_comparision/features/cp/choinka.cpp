#include <vector>
#include <map>
#include <cstdint>
#include <algorithm>
#include <stdio.h>
#include <math.h>
#include <iostream>
#include <queue>

using std::vector;
using std::map;
using std::swap;
using std::min;
using std::cin;
using std::cout;
using std::queue;
typedef uint64_t kolor;

struct Bombki {
    bool zepsute;
    kolor k1;
    kolor k2;
    uint32_t l1;
    uint32_t l2;
};
typedef struct Bombki Bombki;

struct NodePrimitive {
    NodePrimitive* ojciecptr;
    std::vector<NodePrimitive*> dzieci;
    uint32_t rozmiarDrzewa;
    uint32_t numer;
    uint32_t nieodwiedzoneDzieci;
};

NodePrimitive pnodeinit() {
    NodePrimitive res;
    res.ojciecptr = nullptr;
    vector<NodePrimitive*> dzieci;
    res.dzieci = dzieci;
    res.numer = 0;
    res.rozmiarDrzewa = 1;
    res.nieodwiedzoneDzieci = 0;
    return res;
}

Bombki zepsuj() {
    Bombki res;
    res.zepsute = true;
    return res;
}

Bombki zero() {
    Bombki res;
    res.l2 = 0;
    res.k1 = 0;
    res.k2 = 0;
    res.l1 = 0;
    res.zepsute = false;
    return res;
}

Bombki dodaj(Bombki b1, Bombki b2) {
    Bombki res;
    map<kolor, uint32_t> ilosciKolorow;
    if (b1.zepsute || b2.zepsute)
        return zepsuj();
    if (b1.l1 != 0)
        ilosciKolorow[b1.k1] = b1.l1;
    else {
        return b2;
    }
    if (b1.l2 != 0)
        ilosciKolorow[b1.k2] = b1.l2;
    if (b2.l1 != 0) {
        if (ilosciKolorow.find(b2.k1) != ilosciKolorow.end()) {
            ilosciKolorow[b2.k1] = ilosciKolorow[b2.k1] + b2.l1;
        } else {
            ilosciKolorow[b2.k1] = b2.l1;
        }
    } else {
        ilosciKolorow.clear();
        return b1;
    }
    if (b2.l2 != 0) {
        if (ilosciKolorow.find(b2.k2) != ilosciKolorow.end()) {
            ilosciKolorow[b2.k2] = ilosciKolorow[b2.k2] + b2.l2;
        } else {
            ilosciKolorow[b2.k2] = b2.l2;
        }
    }
    if (ilosciKolorow.empty())
        return zero();
    if (ilosciKolorow.size() > 2)
        return zepsuj();
    auto it = ilosciKolorow.begin();
    res.k1 = it->first;
    res.l1 = it->second;
    it++;
    if (it == ilosciKolorow.end()) {
        res.l2 = 0;
        res.k2 = 0;
        res.zepsute = false;
        ilosciKolorow.clear();
        return res;
    }
    res.k2 = it->first;
    res.l2 = it->second;
    if (min(res.l2, res.l1) > 1) {
        ilosciKolorow.clear();
        return zepsuj();
    } else
        res.zepsute = false;
    if (res.l1 < res.l2) {
        swap(res.l1, res.l2);
        swap(res.k1, res.k2);
    }
    ilosciKolorow.clear();
    return res;
}

Bombki kalkuluj(Bombki T[], uint32_t zakres, uint32_t indeks) {
    if (indeks < zakres/4) {
        T[indeks * 2] = kalkuluj(T, zakres, indeks * 2);
        T[indeks * 2 + 1] = kalkuluj(T, zakres, indeks * 2 + 1);
    }
    return dodaj(T[indeks * 2], T[indeks * 2 + 1]);
}

void robimyBinarke(uint32_t Data[], NodePrimitive* pnode, Bombki T[], uint32_t mujindex, uint32_t maxindeks, bool* bul) {
    Data[pnode->numer] = mujindex;
    uint32_t counter = 1;
    for (NodePrimitive* son : pnode->dzieci) {
        robimyBinarke(Data, son, T, mujindex+counter, maxindeks, bul);
        counter += son->rozmiarDrzewa;
    }
}

void dodajDoDrzewa(NodePrimitive* node) {
    if (node == nullptr || node->ojciecptr == nullptr)
        return;
    NodePrimitive* ojciec = node->ojciecptr;
    ojciec->rozmiarDrzewa += node->rozmiarDrzewa;
    ojciec->nieodwiedzoneDzieci--;
    if (ojciec->nieodwiedzoneDzieci == 0)
        dodajDoDrzewa(ojciec);
}

void sumujDoGury(Bombki T[], uint32_t indeks) {
    if (indeks == 0)
        return;
    T[indeks] = dodaj(T[indeks * 2], T[indeks * 2 + 1]);
    sumujDoGury(T, (indeks - indeks%2) / 2);
}

void zmien(uint32_t indeks, kolor nowyKolor, Bombki T[]) {
    T[indeks].k1 = nowyKolor;
    uint32_t nrOjca = indeks - indeks%2;
    nrOjca /= 2;
    sumujDoGury(T, nrOjca);
}

Bombki wyluskaj(Bombki T[], uint32_t currindeks, uint32_t indeksl, uint32_t indeksp, uint32_t lprzedzial, uint32_t pprzedzial) {
    if (lprzedzial == indeksp || indeksl == pprzedzial) {
        return zero();
    }
    if (lprzedzial >= indeksl && pprzedzial <= indeksp) {
        return T[currindeks];
    } else {
        if (!(indeksp < lprzedzial || indeksl > pprzedzial)) {
            uint32_t srodek = (lprzedzial + pprzedzial) / 2;
            return dodaj(wyluskaj(T, currindeks*2, indeksl, indeksp, lprzedzial, srodek), wyluskaj(T, currindeks*2+1, indeksl, indeksp, srodek, pprzedzial));
        } else {
            return zero();
        }
    }
}

int main() {
    uint32_t acc, n1, q1;
    cin >> n1 >> q1;
    const uint32_t n = n1, q = q1;
    uint32_t rozmiarTablicy = 2<<(1 + (int)(floor(log2(n-1))));
    uint32_t prefiks = rozmiarTablicy/2;
    Bombki T[rozmiarTablicy];
    uint32_t Data[n+3];
    NodePrimitive PrimitiveTab[n+3];
    bool U[n+3];
    for (int i = 0; i < rozmiarTablicy; i++) {
        T[i] = zero();
    }
    for (int i = 0; i <= n; i++) {
        U[i] = false;
        PrimitiveTab[i] = pnodeinit();
        PrimitiveTab[i].numer = i;
    }
    for (int i = 2; i <= n; i++) {
        cin >> acc;
        PrimitiveTab[i].ojciecptr = &(PrimitiveTab[acc]);
        PrimitiveTab[acc].dzieci.push_back(&(PrimitiveTab[i]));
        U[acc] = true;
    }
    queue<NodePrimitive> que;
    for (int i = 2; i <= n; i++) {
        if (!U[i]) {
            que.push(PrimitiveTab[i]);
        } else {
            PrimitiveTab[i].nieodwiedzoneDzieci = PrimitiveTab[i].dzieci.size();
        }
    }
    while (!que.empty()) {
        NodePrimitive nodeTop = que.front();
        que.pop();
        dodajDoDrzewa(&nodeTop);
    }
    bool buul = false;
    bool* bul = & buul;
    robimyBinarke(Data, &(PrimitiveTab[1]), T, 0, n+1, bul);
    if (*bul) {
        return 0;
    }
    kolor kacc;
    for (int i = 1; i <= n; i++) {
        cin >> kacc;
        T[prefiks + Data[i]].l1 = 1;
        T[prefiks + Data[i]].k1 = kacc;
    }
    T[1] = kalkuluj(T, rozmiarTablicy, 1);
    char polecenie;
    uint32_t v;
    kolor k;
    for (int i = 0; i < q; i++) {
        cin >> polecenie;
        if (polecenie == 'z') {
            cin >> v >> k;
            zmien(Data[v] + prefiks, k, T);
        } else {
            cin >> v;
            Bombki b = wyluskaj(T, 1, Data[v], Data[v] + PrimitiveTab[v].rozmiarDrzewa, 0, prefiks);
            if (b.zepsute) {
                cout << "NIE\n";
            } else {
                cout << "TAK\n";
            }
        }
    }
}