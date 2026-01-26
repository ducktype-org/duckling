Query wymaga posprzątania:

klasa context ma w sobie za dużo logiki, szczególnie .query.
To powinna być odpowiedzialność czegoś ala standardQueryEntry. Jest na pewno potrzeba "dwóch warstw", ale te warstwy powinny być jawne.



entryPoint efektywnie robi bypass na zewnętrzną warstwę – to jest ok, ale to trzeba jakoś jawnie napisać

