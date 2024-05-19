## Moduł do wypisywania komunikatów na konsolę.

### Moduł implementuje:

1. `typedef uint32_t LevelType`

2. `enum MessageType` (Error = 0, Debug = 1, Note = 2, Hint = 3, ...)
    Powinna mieć określony typ, powinna być możliwa konwersja na odpowiedniego int-a (może być to funkcja).

3. `constexpr usize typeCount` określającą ilość typów wiadomości (kod powinien zakładać, że ta wartość może się zmienić)

4. `enum Color`, zmienną `Color defaultColor = White`.

5. Typ `PrinterContent`, który póki co jest po prostu równy `std::string` (czyli na razie `typedef`).

6. Typ `ColoredContent`, który utrzymuje wiadomość typu `PrinterContent`, oraz kolor typu `Color`.

7. funkcję (ewentualnie może to być konstruktor) `ColoredContent paint(Color color, PrinterContent str)`.
    Dodatkowo udostępnia też funkcje `default, red, blue, ...`, które działają jak paint, ale z danym kolorem.

8. Typ `Message`, który posiada typ typu `enum MessageType`, poziom typu `LevelType`,
    Komunikat można konstruować z `std::initializer_list<ColoredContent>` lub `std::initializer_list<PrinterContent>`.
    (Elementy `PrinterContent` są automatycznie zamieniane na `ColoredContent` z kolorem domyślnym).
    Wiadomość utrzymuje listę `ColoredContent` (prawdopodobnie `std::vector<ColoredContent>`).
    W przyszłości: konturowanie z `std::initializer_list<std::variant<ColoredContent, PrinterContent>>`.
    
9. Typ `MessagePack`, który efektywnie jest wektorem komunikatów. (póki co może być `typedef`)

10. Typ `Console`, do którego można dodawać `MessagePack`. 
     Utrzymuje on:
    
     * (1) wektor `MessagePack`
     * (2) wartość `LevelType` dla każdego typu komunikatu, który określa minimalny poziom danego typu wiadomości.
         (powinien być możliwy łatwy refactor na maksymalny)
         (prawdopodobnie `std::array<LevelType, typeCount>`)
     * (3) Wartość `generalMax` typu `usize` określającą całkowitą maksymalną liczbę komunikatów.
     * (4) Wartości typu `usize` określającą maksymalną liczbę komunikatów każdego typu.
         (prawdopodobnie `std::array<usize, typeCount>`)

     Posiada on też funkcję `printErr`, która wypisuje wszystkie komunikaty na `stderr` (z kolorami), ale:
      - pojedynczy MessagePack jest traktowany jako wiele oddzielnych komunikatów w nich zawartych.
      - pomija on komunikat jeżeli typ komunikatu powinien być ignorowany z powodu (2).
      - kończy jeżeli liczba wypisanych komunikatów przekroczy `generalMax`.
      - omija komunikat jeżeli liczba wypisanych komunikatów danego typu przekroczy odpowiadającą wartość.
      - dwa powyższe przypadki skutkują wypisaniem dodatkowego, nie wliczanego do limitów, krótkiego
       komunikatu wyjaśniającego co się stało.
    
     Dodatkowo ma funkcję `clear`, która usuwa wszystkie komunikaty utrzymywane przez konsolę.

11. W przyszłości: typ `MessageTemplate`

12. W przyszłości: move semantic tam gdzie powinno być

13. W przyszłości: optymalizacje.. todo


### Z tego udostępnia (w .hpp):

todo