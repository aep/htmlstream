#include "htmlstream.h"
#include <stdio.h>
#include <unistd.h>

static int indent = 0;
static bool was_open = false;
void pretty_open_cb(void *parg, const char *tag, int len)
{
    if (was_open) {
        was_open = false;
        printf(">\n");
    }
    ++indent;
    printf("%*s<%.*s", indent, " ", len, tag);
    was_open = true;
}
void pretty_close_cb(void *parg, const char *tag, int len)
{
    if (was_open) {
        was_open = false;
        printf(">\n");
    }
    --indent;
    printf("%*s</%.*s>\n", indent, " ", len, tag);
}
void pretty_text_cb(void *parg, const char *text, int len)
{
    if (was_open) {
        was_open = false;
        printf(">\n");
    }
    printf("%*s%.*s\n", indent, " ", len, text);
}

void pretty_attr_cb(void *parg, const char *key, int key_l, const char *val, int val_l)
{
    printf(" %.*s='%.*s'", key_l, key, val_l, val);
}

int main(int argc, char **argv)
{
    htmlstream_t t;
    htmlstream_init(&t);
    t.open_cb = pretty_open_cb;
    t.close_cb = pretty_close_cb;
    t.text_cb  = pretty_text_cb;
    t.attr_cb  = pretty_attr_cb;

    for (;;) {
        char buf[1024];
        int l = read(fileno(stdin), buf, 1024);
        if (l < 1)
            return 0;
        htmlstream_feed(&t, buf, l);
    }

}
