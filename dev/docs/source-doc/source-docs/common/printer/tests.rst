=====
Tests
=====

Due to printer nature as a module it's hard to test its functionality, since it's about the visual output of a terminal. Because of this, tests are quite limited and test only basic functionalities.

Test
====

| Test constructs a simple :code:`MessageContent` and adds it to a :code:`Console` twice testing multiple constructors and methods along the way.
| First it tests whether outputting printing the full message yields the right result.
| Then it tests reaching maximum amount for a type of message. 
| Then reaching maximum amount of all messages. 
| Then ensures that nothing is printed if minimal level of a message is set sufficiently high.
