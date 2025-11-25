# Token Stream Requirements

# class TokenStream:

* `where` - the position of the stream; the current token is `token_data.tokens[where]`. We do not touch other things from `token_data`.
The stream advances forward, meaning subsequent tokens are at positions `where+1`, `where+2`, ...

* `TokenStream(TokenData&& token_data)` - simple constructor.
* `Token& next();` - returns the token currently pointed to by `where`, moves `where` forward.
* `Token& peek(usize fwd = 0) const;` returns the `fwd`-th token in the stream.
* `void skip(usize n = 1);` - advances the stream `n` positions forward.
* `TokenStreamState state() const;` - returns the current state of the stream.
* `void restore(TokenStreamState state);` - restores a given state of the stream.

* isKeyword, asKeyword, isSpecial, asSpecial - forwards methods from `peek(fwd)`.
* `bool isOperator(usize fwd = 0) const;` - checks if `peek(fwd).getType() == Token::Type::Operator`.
* `bool isOperator(base::StrID oper, usize fwd = 0) const;` - checks if `isOperator(fwd)` and `peek(fwd).isStr(oper)`.

* `usize size() const;` - returns the remaining number of tokens in the stream.

When anything looks out of bounds, a `sentinel` should be returned (the `makeSentinel` method does not exist yet, but it will).
