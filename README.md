simple html stream parser for html in C that does not malloc anything


Written from practical tests rather then the actual html spec,
because barely anyone who makes websites did ever read the html spec anyway.
It can read pretty bad html, but will not fix the structure by adding missing end tags
or guessing what the user wanted, like modern browsers do.

All tags and attributes are lower cased. Text remains original case.
( #define tolower(x) (x)  if you dont want that )

UTF8 is not handled. specifically special utf8 space stuff is considered to be not space.
Browser behaviour may be different.
