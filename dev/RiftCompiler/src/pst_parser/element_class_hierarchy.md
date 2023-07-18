Forward deklaracje są zapisane w odziemnym pliku/plikach

Klasy mogą mieć statyczną metodę `parse`, która parsuje dowolne wyrażenie, zależnie na jakie tokeny trafi.

**Element**  
│ Główna klasa, która definiuje wspólny interface parse-owania i alokator-ów.  │ `inline void* operator new(size_t size);` - to robienia custom-owych alokator-ów w przyszłości
│ `inline void operator delete(void* p)` - j.w.
│ `virtual @TODO dprint() const = 0;` - Creates printer message of element debug representation
│
├─ **StmtList** final
│  │ Efektywnie `std::vector<Stmt*>`, ale implementujące `Element`
│
├─ **Stmt** 
│  │ Element, który może występować samodzielnie
│  │  
│  │   
│  ├─ **Expr** final  
│  │  │ Dowolne wyrażenie operatorów.
│  │  │ Specjalnie ogarnia operator lambdy `=>`, i być może `:=`, `:`.    
│  │  │ Może wprowadzać nowe symbole.  
|  |  | Może zawierać bloki kodu (lambdy)
│  │  
│  │  
│  ├─ **Decl**  
│  │  │ Wszelkie takie jak funkcje, pętle, bloki kodu.  
│  │  │ Mogą deklarować od 0 do dowolnej liczby symboli.
│  │  │ Tutaj będzie prawdopodobnie większość logiki jak i mnóstwo boilerplate-u (na szczęście powtarzalnego)
│  │  │  
│  │  ├─ **Fun**
│  │  │  │ Funkcja 
│  │  │  │ todo 
│  │  │ 
│  │  ├─ Wszelkie deklaracje (while, for, var?, let?, block, macro, with, loop ...)  
│  │  │  │ @IDEA: Gdyby np `for` miał parę wariantów, 
│  │  │  │ to pewnie warto aby były to podklasy wspólnego interface-u `For`,
│  │  │  │ bardzo możliwe, że pustego 
│  │  
│  │  
│  ├─ **Action**  
│  │  │ Struktury takie jak `return 2;`, `break A;`.  
│  │  │ Zazwyczaj będzie to słowo kluczowe, po którym występuje `Expr`.  
│  │  │  
│  │  ├─ **Return**
│  │  ├─ **Break** 
│  │  ├─ **Continue** 
│  │  ├─ **Redo** 
│  │  ├─ **Exit** @TODO: chcemy tego typu? 
│  │  ├─ ...
│  │  
│  │  
│  ├─ **Attr** final  
│  │  │ @TODO: Może `NotStmt`?
│  │  │ Pojedynczy atrybut `@name(params)` albo `@name`  
│  │  
│  │  
│  ├─ **AttrList** final  
│  │  │ Efektywnie `std::vector<Attr>`, ale implementujące `parse` - pewnie korzysta z generatora
│  │  
│  
├─ **NotStmt**
│  │ Klasa, która służy na posegregowanie w jednym miejscu bloczków budujących elementy, które nie występującą samodzielnie
│  │  
│  ├─ **OptionalName** 
│  │  │ Czyta identifier, jeśli jest, inaczej nic nie czyta. Zapisuje swój stan  
│  │
│  ├─ **ParamList**
│  │  │ Lista parametrów (funkcji, makra, atrybutu, ...)  
|  |
│  ├─ **ArgList**
│  │  │ Lista argumentów (funkcji, makra, atrybutu, ...)
|  | 
|  ├─ **CodeBlock**
|  |  | @TODO: może to jest `Stmt`?
|  |  | Czyta `StmtList` w `{}`.
|  |
|  ├─ **CodeBlockOrStmt**
|  |  | @TODO: może to jest `Stmt`?
|  |  | Czyta `StmtList` w `{}`, lub jedno `Stmt` w niczym.
|  |
|  ├─ Wszelkie `NotStmt` typu fragmenty for-a



**Generators**
| @TODO: Czy powinny dziedziczyć po `Element` -- czy mają wspólny interface
| Interface generatorów, typu `template<...> class ParseInOrder`, albo `template<T, Separator> class ListOf`
| @IDEA: Może nie klasy?
| Korzystając z generatorów wciąż piszemy nową klasę w hierarchii `Element`. 
| Raczej chcemy dużo korzystać z generatorów
| Może ona zawierać generatory jako pola, zmienne, ...  
|
├─ **ElemArray<T, uint32_t count>**
├─ **ElemList<T>**
├─ **Optional<T>**
├─ **ElemTuple<T...>**
├─ **SimpleExprAction<Key>**
├─ @IDEA: Jakieś rzeczy do cięcia tokenów na fragmenty
├─ @IDEA: Lista, ale ogarniająca separator (np `,`). Może być przydatne do wprowadzania potem `For A(), B(), C() {}` i podobnych
├─ ...
