
Jak jest wprowadzana interaktywność w nowej DIA?

1. Alternative content. 

Można kliknąć na napis i wyświetli się alternative content tekstu.
Np. pełna nazwa typu zamiast aliasu.

2. Dodatkowe komunikaty załączone do obiektu

Możemy określić, że w komunikacie o treści "zmienan x ma typ T1 a oczekuje T2", 
część napisu "x" jest obiektem.

Obiekty mogą posiadać własne dodatkowe komunikaty 
Te dodatkowe komunikaty można wyświetlać na rządanie. Przykład - gdzie jest zadeklarowana zmienna x.
Gdzie jest template instantiation. Gdzie został element zaimportowany.

3. Powiązania grafowe.

Komunikat może zdefiniować wyjściowe powiązania grafowe. Link ma pewien tekst,
który wskazuje na to, że jak kliekniemy na ten tekst to nas przeniesie do docelowego komunikatu.

Przykładem może być overload resolution, gdzie wypisanie dlaczego jakiś kandydat został odrzucony może
zostać zaimplementowane jako link grafowy.



The function call ambiguous match:

x(a,b,c);

Found 2 exact matches:
- match 1
```
fun x(int a, int b, int c) -> i64 {
    ...
}
```
- match 2
```
fun x(int a, int d, int c) -> i32 {
```

Found 1 coercion match:

fun x(int a, int d, bool c) -> i32 {
                    ------ coercion from int to bool

Found 1 match fail:

fun x(int a, int b, string c) -> i32 {
                    -------- no conversion found from int to string (aka vector<char>)




error[E0382]: use of moved value: `order`
--> src/main.rs:200:13
|
196 | let order = get_order();
| ----- ----------- this reinitialization might
| | get skipped
| move occurs because `order` has type `Order`,
| which does not implement the `Copy` trait
197 | if yes_no("Would you like to save this order?") {
198 | van_binh.add_customer(name, order);
| ----- value moved here
199 | }
200 | order
| ^^^^^ value used here after move
|
note: consider changing this parameter type in method `add_customer`
to borrow instead if owning the value isn't necessary
--> src/main.rs:119:62
|
119 | fn add_customer(&mut self, name: String, favorite_order: Order) {
| ------------ in this method ^^^^^
this parameter takes ownership of the value
help: consider cloning the value if the performance cost is acceptable
|
198 | van_binh.add_customer(name, order.clone());
| ++++++++


error: unable to deduce lambda return type from multiple return statements
note: lambda at src/main.cpp:73 has returns of incompatible types:
      return Node(id);          // returning Node
      return std::nullopt;      // returning std::optional<Node>
note: no common type exists between ‘Node’ and ‘std::optional<Node>’
help: specify an explicit return type:
      [&]() -> std::optional<Node> { ... }

