#include "htmlstream.h"
#include <stdio.h>


static int expected_cb_count = 0;
static int actual_cb_count   = 0;

const char *expected_open;
void test_htmlstream_open_cb(void *parg, const char *tag, int len)
{
    actual_cb_count++;
    if (len == strlen(expected_open) && strncmp(expected_open, tag, len) == 0) {
        printf("[PASS %d] open <%.*s> == <%s> \n", actual_cb_count, len, tag, expected_open);
    } else {
        printf("[FAIL %d] open <%.*s> != <%s> \n", actual_cb_count, len, tag, expected_open);
        exit(2);
    }
}

const char *expected_close;
void test_htmlstream_close_cb(void *parg, const char *tag, int len)
{
    actual_cb_count++;
    if (len == strlen(expected_close) && strncmp(expected_close, tag, len) == 0) {
        printf("[PASS %d] close </%.*s> == </%s> \n", actual_cb_count, len, tag, expected_close);
    } else {
        printf("[FAIL %d] close </%.*s> != </%s> \n", actual_cb_count, len, tag, expected_close);
        exit(2);
    }
}

const char *expected_key;
const char *expected_val;
void test_htmlstream_attr_cb(void *parg, const char *key, int key_l, const char *val, int val_l)
{
    actual_cb_count++;
    if (key_l != strlen(expected_key) || strncmp(expected_key, key, key_l) != 0) {
        printf("[FAIL %d] key '%.*s' != '%s' \n", actual_cb_count, key_l, key, expected_key);
        exit(2);
        return;
    }
    if (expected_val) {
        if (val_l != strlen(expected_val) || strncmp(expected_val, val, val_l) != 0) {
            printf("[FAIL %d] val '%.*s' != '%s' \n", actual_cb_count, val_l, val, expected_val);
            exit(2);
            return;
        }
    } else {
        if (val_l) {
            printf("[FAIL %d] unexpected val '%.*s' \n", actual_cb_count, val_l, val);
            exit(2);
            return;
        }
    }
    printf("[PASS %d] %s=%s \n", actual_cb_count, expected_key,expected_val);
}

void test_htmlstream_text_cb(void *parg, const char *text, int len)
{
    actual_cb_count++;
    if (len == strlen(expected_val) && strncmp(expected_val, text, len) == 0) {
        printf("[PASS %d] text '%.*s' \n", actual_cb_count, len, text, expected_val);
    } else {
        printf("[FAIL %d] text '%.*s' != '%s' \n", actual_cb_count, len, text, expected_val);
        exit(2);
    }
}

#define TEST_OPEN(NAME, HTML) \
{\
    ++expected_cb_count; \
    expected_open = NAME; \
    t.open_cb = test_htmlstream_open_cb; \
    htmlstream_feed(&t, HTML, strlen(HTML));\
    t.open_cb = 0;\
}

#define TEST_CLOSE(NAME, HTML) \
{\
    ++expected_cb_count; \
    expected_close = NAME; \
    t.close_cb = test_htmlstream_close_cb; \
    htmlstream_feed(&t, HTML, strlen(HTML));\
    t.close_cb = 0;\
}
#define TEST_ATTR(K,V, HTML) \
{\
    ++expected_cb_count; \
    expected_key = K; \
    expected_val = V; \
    t.attr_cb = test_htmlstream_attr_cb; \
    htmlstream_feed(&t, HTML, strlen(HTML));\
    t.attr_cb = 0; \
}
#define TEST_TEXT(TEXT, HTML) \
{\
    ++expected_cb_count; \
    expected_val = TEXT; \
    t.text_cb = test_htmlstream_text_cb; \
    htmlstream_feed(&t, HTML, strlen(HTML));\
    t.text_cb = 0; \
}


int main(int argc, char**argv)
{
    htmlstream_t t;
    htmlstream_init(&t);

    TEST_OPEN("derp",     "< dErp kacka=wuast  > "); //1
    TEST_OPEN("com-plete", "< Com-Plete string> ");
    TEST_OPEN("foo", "<><<foo>");
    TEST_OPEN("bar", "<> < <bar c >derp>>");
    TEST_OPEN("dd", "<> << dd>");

    TEST_CLOSE("jop", "</joP>"); //6
    TEST_CLOSE("closed", "< /  closed not=true>");
    TEST_OPEN ("not", "< / < not closed not=true>");
    TEST_CLOSE("closed", "< / closed < new");
    TEST_OPEN ("new", " shit on the block");

    TEST_OPEN ("html", "<html sucks>"); //11
    TEST_ATTR("color", "red", "<derp color=red/>");
    TEST_ATTR("color", "green", "<derp color=green />");
    TEST_ATTR("you", "awe", "<derp you = awe<some >");
    TEST_ATTR("google",0 , "<ok google google =");

    TEST_ATTR("google","that", "that/>"); //16
    TEST_ATTR("you", "knorke", "<me you = knorke </me>");
    TEST_ATTR("escape", "hell ' <3", "<omg escape=hel'l \\' <3' ");
    TEST_ATTR("schnitzel", 0, "schnitzel \"oh god\"");
    TEST_ATTR("oh god", 0, ">");

    TEST_OPEN("!doctype", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">"); //21
    TEST_OPEN("comments", "<!-- <stuff> --><comments>");
    TEST_ATTR("http-equiv", "content-type", "<meta  http-equiv=\"Content-Type\" ");
    TEST_ATTR("content", "text/html; charset=utf8", "content=\"text/html; charset=utf8\">");
    TEST_ATTR("complete tag", "is complete", "<a complete\" tag\" = 'is<!-- not--> complete'>");
    TEST_ATTR("complete <!", " complete d", "<a 'complete <!' = ' <!-- not-->complete d' >");

    TEST_TEXT("pew pew", "<lazer>pew pew</lazer>");
    TEST_TEXT("yolo", "<yoo lo>yo<!--yo -->lo< / lo >");
    TEST_TEXT("turbo 'swag", "<ok >    turbo 'swag  </d>");






    if (expected_cb_count == actual_cb_count) {
        printf("[PASS] %d/%d passed\n", actual_cb_count, expected_cb_count);
    } else {
        printf("[FAIL] %d/%d missing \n", expected_cb_count - actual_cb_count, expected_cb_count);
        exit(3);
    }
}
