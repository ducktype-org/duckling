# class Token stream:

* `where` - pozycja stream-a, aktualny token to `token_data.tokens[where]`. Innych rzeczy z `token_data` nie tykamy
Stream idzie do przodu, czyli kolejne tokeny są na pozycjach `where+1`, `where+2, `...

* `TokenStream(TokenData&& token_data)` - prosty konstruktor
* `Token& next();` - zwraca token na który aktualnie patrzy `where`, przesuwa `where` do przodu
* `Token& peek(size_t fwd = 0) const;` zwraca `fwd`-ty token w stream-ie
* `void skip(std::size_t n = 1);` - przesuwa stream - `n` do przodu
* `TokenStreamState state() const;` - zwraca aktualny stan stream-u
* `void restore(TokenStreamState state);` - przywraca dany stan stream-u 

* isKeyword, asKeyword, isSpecial, asSpecial - forward metod z `peek(fwd)`
* `bool isOperator(size_t fwd = 0) const;` - sprawdza czy `peek(fwd).getType() == Token::Type::Operator`
* `bool isOperator(base::StrId oper, size_t fwd = 0) const;` - sprawdza czy `isOperator(fwd)` oraz`peek(fwd).isStr(oper)`

* `size_t size() const;` - zwraca pozostałą liczbę token-ów w streamie

Gdy cokolwiek patrzy poza zakres to powinien być zwracany `sentinel` (póki co nie istnieje metoda makeSentinel, ale będzie istnieć).