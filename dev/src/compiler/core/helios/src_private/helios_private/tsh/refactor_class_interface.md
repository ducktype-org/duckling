[[nodiscard]] bool isTriviallyDestructible(query::Context& ctx) const override;
[[nodiscard]] bool isDefaultConstructible(query::Context&) const override;
[[nodiscard]] bool isTriviallyZeroInitializable(query::Context&) const override;
[[nodiscard]] bool isCopyable(query::Context&) const override;
[[nodiscard]] bool isTriviallyCopyable(query::Context&) const override;

[[nodiscard]] bool carriesInformation(query::Context& ctx) const override {

Mamy ClassSymbolData QueryClassSymbolData, które jest podstawowym mocnym źródłem danych.


Mamy QueryInterfaceOfClass