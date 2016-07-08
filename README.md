[![Build Status](https://travis-ci.org/aep/htmlstream.svg?branch=master)](https://travis-ci.org/aep/htmlstream)
[![NoDependencies](http://aep.github.io/images/no-dependencies.svg)](#)


htmlstream
------------

simple html stream parser for html in C that does not malloc anything, in a single header file only. no dependencies.

It can read pretty bad html, but will not fix the structure by adding missing end tags 
or guessing what the user wanted, like modern browsers do. 
It also does not close void tags like &lt;br&gt;

All tags and attributes are lower cased. Text remains original case.
( #define tolower(x) (x)  if you dont want that )

UTF8 is not handled. specifically special utf8 space stuff is considered to be not space.
Browser behaviour may be different.


Usage
-------

```C

#include "htmlstream.h"

void my_open_cb(void *parg, const char *tag, int len);
void my_close_cb(void *parg, const char *tag, int len);
void my_text_cb(void *parg, const char *text, int len);
void my_attr_cb(void *parg, const char *key, int key_l, const char *val, int val_l);

htmlstream_t t;
htmlstream_init(&t);
t.parg     = something;
t.open_cb  = my_open_cb;
t.close_cb = my_close_cb;
t.text_cb  = my_text_cb;
t.attr_cb  = my_attr_cb;


char mybuf = "<html> etc";
htmlstream_feed(&t, mybuf, strlen(mybuf));

```
