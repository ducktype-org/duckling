========
Examples
========

Example creates a :code:`Console` and adds messages of various types and importance levels to it.

First message is standard, second one is "RAINBOW" letters colored in a rainbow pattern, third showcases default letter colors, fourth showcases background colors and fifth showcases default background colors.

| First, all messages are printed normally to showcase the output.
| Then, minimum importance of a message level is set to 4, so that only the fifth message shows up when printing console output.
| Then, amount of hint type messages is limited to 1 so the third message which is the second hint is not printed.
| Then, maximum amount of all messages is set to one so only the first message is printed.
| And lastly, the console is cleared of all messages and adding single messages to consoles through initializer lists is showcased. Also showcasing that console settings are not reset upon clearing messages from it.

Here is a screenshot of the console output:

.. image :: exampleoutput.png
	:width: 100% 
