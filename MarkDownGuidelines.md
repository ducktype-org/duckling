# How to write .md for GitHub

1. Use GitHub docs  
   <https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax>

2. Put new lines after headers and before lists, tables, ...  
   Example:
   ~~~markdown
   # Header
    <!-- Here -->
   Text
    <!-- Here -->
    * First
    * Second
   ~~~

3. Use `**Bold**`, **Bold**
4. Use `_Italic_`, _Italic_
5. Use `**_BoldItalic_**`, **_BoldItalic_**

6. Adding new line  
   Double space at the end of line or `<br>` anywhere else.  
   Example:
   ~~~markdown  
   A  <!-- Double space -->
   B<!-- No Double space -->
   C
   <br> <!-- It will break things like lists -->
   ~~~

7. Indentation  
   To the level of parent element (with spaces) or 
   double space if indenting to parent doesn't make sense or break things
   <!-- See https://github.com/github/markup/issues/1084 -->

8. Inline code  
  `code here`, `int a` <!-- Can't find working way of adding highlight to inline code -->
                      <!-- see: https://github.com/vuejs/vuepress/issues/1212 -->

9. Code blocks  
   ~~~~~
   code here
   ~~~~~
   ~~~~~cpp
   int a;
   ~~~~~

10. Links  
    <https://github.com/rift-lang/>  
    [Rift Lang](https://github.com/rift-lang/)  
    [How to write .md](#how-to-write-md-for-github)
