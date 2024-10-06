#include <bits/stdc++.h>
using namespace std;
bool T[1300000];
int main(){
    int n;
    cin >> n;
    for (int i = 2; i<1141; i++)
        for (int j=i*i; !T[i] && j<1300000; j+=i)
            T[j]=1;
    for (int i=2; n; n-=1-T[i++])
        if (!T[i])
            cout << i << "\n";
}