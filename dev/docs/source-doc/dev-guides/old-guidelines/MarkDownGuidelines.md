# How to write .md for GitHub

1. Use GitHub docs  
   <https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax>

1. Put new lines after headers and before lists, tables, ...  
   Example:
   ~~~markdown
   # Header
    <!-- Here -->
   Text
    <!-- Here -->
    * First
    * Second
   ~~~

1. Use `**Bold**`, **Bold**
1. Use `_Italic_`, _Italic_
1. Use `**_BoldItalic_**`, **_BoldItalic_**

1. Adding new line  
   Double space at the end of line or `<br>` anywhere else.  
   Example:
   ~~~markdown  
   A  <!-- Double space -->
   B<!-- No Double space -->
   C
   <br> <!-- It will break things like lists -->
   ~~~

1. Indentation  
   To the level of parent element (with spaces) or 
   double space if indenting to parent doesn't make sense or break things
   <!-- See https://github.com/github/markup/issues/1084 -->

1. Inline code  
  `code here`, `int a` <!-- Can't find working way of adding highlight to inline code -->
                      <!-- see: https://github.com/vuejs/vuepress/issues/1212 -->

1. Code blocks  
   ~~~~~
   code here
   ~~~~~
   ~~~~~cpp
   int a;
   ~~~~~

1. Links  
   <https://github.com/rift-lang/>  
   [Rift Lang](https://github.com/rift-lang/)  
   [How to write .md](source-doc/dev-guides/old-guidelines/MarkDownGuidelines:how-to-write-md-for-github)
