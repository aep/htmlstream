#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define HTMLSTREAM_TAG_BUFFER_SIZE 100
#define HTMLSTREAM_STATE_TEXT            0
#define HTMLSTREAM_STATE_TAG_START       1
#define HTMLSTREAM_STATE_OPENING_TAG     2
#define HTMLSTREAM_STATE_OPENING_TAG_END 3
#define HTMLSTREAM_STATE_TAG_START_SLASH 4
#define HTMLSTREAM_STATE_CLOSING_TAG     5
#define HTMLSTREAM_STATE_ATTR_KEY        6
#define HTMLSTREAM_STATE_ATTR_KEY_QUOTE  7
#define HTMLSTREAM_STATE_ATTR_KEY_END    8
#define HTMLSTREAM_STATE_ATTR_EQ         9
#define HTMLSTREAM_STATE_ATTR_VAL       10
#define HTMLSTREAM_STATE_ATTR_VAL_QUOTE 11


typedef void(*htmlstream_open_cb)(void *parg, const char *tag, int len);
typedef void(*htmlstream_close_cb)(void *parg, const char *tag, int len);
typedef void(*htmlstream_attr_cb)(void *parg,
        const char *key, int key_len,
        const char *val, int val_len);
typedef void(*htmlstream_text_cb)(void *parg, const char *text, int len);

typedef struct
{
    htmlstream_open_cb  open_cb;
    htmlstream_close_cb close_cb;
    htmlstream_attr_cb  attr_cb;
    htmlstream_text_cb  text_cb;
    void *parg;

    int state;

    char tag_buffer[HTMLSTREAM_TAG_BUFFER_SIZE];
    int tag_buffer_at;
    char val_buffer[HTMLSTREAM_TAG_BUFFER_SIZE];
    int val_buffer_at;

    char quote;
    bool escaped;

    int comment;

} htmlstream_t;


void htmlstream_init(htmlstream_t *t)
{
    memset(t, 0, sizeof(htmlstream_t));
}

static void htmlstream_internal_feed(htmlstream_t *t, const char c)
{
    switch (t->state) {
        case HTMLSTREAM_STATE_TEXT:
            if (c == '<') {
                t->state = HTMLSTREAM_STATE_TAG_START;
                if (t->text_cb) {
                    t->text_cb(t->parg, t->val_buffer, t->val_buffer_at);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else {
                if (t->val_buffer_at < HTMLSTREAM_TAG_BUFFER_SIZE) {
                    t->val_buffer[t->val_buffer_at++] = c;
                }
            }
            break;
        case HTMLSTREAM_STATE_TAG_START:
            if (c == '<') {
                //trash = <<
            } else if (c == '>') {
                //trash = <>
                t->state = HTMLSTREAM_STATE_TEXT;
                t->val_buffer_at = 0;
            } else if (c == '/') {
                t->state = HTMLSTREAM_STATE_TAG_START_SLASH;
            } else if (isspace(c)) {
                return;
            } else {
                t->state = HTMLSTREAM_STATE_OPENING_TAG;
                t->tag_buffer[0] = tolower(c);
                t->tag_buffer_at = 1;
            }
            break;
        case HTMLSTREAM_STATE_OPENING_TAG:
            if (c == '>') {
                t->state = HTMLSTREAM_STATE_TEXT;
                if (t->open_cb) {
                    t->open_cb(t->parg, t->tag_buffer, t->tag_buffer_at);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (isspace(c)) {
                t->state = HTMLSTREAM_STATE_OPENING_TAG_END;
                if (t->open_cb) {
                    t->open_cb(t->parg, t->tag_buffer, t->tag_buffer_at);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else {
                if (t->tag_buffer_at < HTMLSTREAM_TAG_BUFFER_SIZE) {
                    t->tag_buffer[t->tag_buffer_at++] = tolower(c);
                }
            }
            break;
        case HTMLSTREAM_STATE_TAG_START_SLASH:
            if (c == '>') {
                //trash: </>
                t->state = HTMLSTREAM_STATE_TEXT;
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (c == '<') {
                //trash: </<
                t->state = HTMLSTREAM_STATE_TAG_START;
            } else if (isspace(c)) {
                return;
            } else{
                t->state = HTMLSTREAM_STATE_CLOSING_TAG;
                t->tag_buffer[0] = tolower(c);
                t->tag_buffer_at = 1;
            }
            break;
        case HTMLSTREAM_STATE_CLOSING_TAG:
            if (c == '>') {
                t->state = HTMLSTREAM_STATE_TEXT;
                if (t->close_cb) {
                    t->close_cb(t->parg, t->tag_buffer, t->tag_buffer_at);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (c == '<') {
                //probably missing > of end tag = </ bla<
                t->state = HTMLSTREAM_STATE_TAG_START;
                if (t->close_cb) {
                    t->close_cb(t->parg, t->tag_buffer, t->tag_buffer_at);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (isspace(c)) {
                t->state = HTMLSTREAM_STATE_OPENING_TAG_END;
                if (t->close_cb) {
                    t->close_cb(t->parg, t->tag_buffer, t->tag_buffer_at);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else {
                if (t->tag_buffer_at < HTMLSTREAM_TAG_BUFFER_SIZE) {
                    t->tag_buffer[t->tag_buffer_at++] = tolower(c);
                }
            }
            break;
        case HTMLSTREAM_STATE_OPENING_TAG_END:
            if (c == '<') {
                //missing >  before <
                t->state = HTMLSTREAM_STATE_TAG_START;
            } else if (c == '>') {
                t->state = HTMLSTREAM_STATE_TEXT;
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (isspace(c)) {
                return;
            } else if (c == '/') {
                //thrash
            } else if (c == '\'' || c == '"') {
                t->state = HTMLSTREAM_STATE_ATTR_KEY_QUOTE;
                t->quote = c;
                t->tag_buffer_at = 0;
            } else {
                t->state = HTMLSTREAM_STATE_ATTR_KEY;
                t->tag_buffer[0] = tolower(c);
                t->tag_buffer_at = 1;
            }
            break;
        case HTMLSTREAM_STATE_ATTR_KEY:
            if (c == '<') {
                t->state = HTMLSTREAM_STATE_TAG_START;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at, 0, 0);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (c == '>') {
                t->state = HTMLSTREAM_STATE_TEXT;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at, 0, 0);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (isspace(c)) {
                t->state = HTMLSTREAM_STATE_ATTR_KEY_END;
            } else if (c == '\'' || c == '"') {
                t->state = HTMLSTREAM_STATE_ATTR_KEY_QUOTE;
                t->quote = c;
            } else if (c == '=') {
                t->state = HTMLSTREAM_STATE_ATTR_EQ;
            } else {
                if (t->tag_buffer_at < HTMLSTREAM_TAG_BUFFER_SIZE) {
                    t->tag_buffer[t->tag_buffer_at++] = tolower(c);
                }
            }
            break;
        case HTMLSTREAM_STATE_ATTR_KEY_QUOTE:
            if (c == '\\') {
                t->escaped = true;
            } else {
                if (c == t->quote && !t->escaped) {
                    t->state = HTMLSTREAM_STATE_ATTR_KEY;
                } else {
                    if (t->tag_buffer_at < HTMLSTREAM_TAG_BUFFER_SIZE) {
                        t->tag_buffer[t->tag_buffer_at++] = tolower(c);
                    }
                }
                t->escaped = false;
            }
            break;

        case HTMLSTREAM_STATE_ATTR_KEY_END:
            if (c == '<') {
                t->state = HTMLSTREAM_STATE_TAG_START;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at, 0, 0);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (c == '>') {
                t->state = HTMLSTREAM_STATE_TEXT;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at, 0, 0);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (isspace(c)) {
            } else if (c == '=') {
                t->state = HTMLSTREAM_STATE_ATTR_EQ;
            } else if (c == '\'' || c == '"') {
                t->state = HTMLSTREAM_STATE_ATTR_KEY_QUOTE;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at, 0, 0);
                }
                t->quote = c;
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else {
                t->state = HTMLSTREAM_STATE_ATTR_KEY;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at, 0, 0);
                }
                t->tag_buffer[0] = tolower(c);
                t->tag_buffer_at = 1;
            }
            break;
        case HTMLSTREAM_STATE_ATTR_EQ:
            if (c == '<') {
                t->state = HTMLSTREAM_STATE_TAG_START;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at, 0, 0);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (c == '>') {
                t->state = HTMLSTREAM_STATE_TEXT;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at, 0, 0);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (c == '=') {
            } else if (isspace(c)) {
            } else if (c == '\'' || c == '"') {
                t->state = HTMLSTREAM_STATE_ATTR_VAL_QUOTE;
                t->val_buffer_at = 0;
                t->quote = c;
            } else {
                t->state = HTMLSTREAM_STATE_ATTR_VAL;
                t->val_buffer[0] = tolower(c);
                t->val_buffer_at = 1;
            }
            break;
        case HTMLSTREAM_STATE_ATTR_VAL:
            if (c == '<') {
                t->state = HTMLSTREAM_STATE_TAG_START;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at,
                            t->val_buffer, t->val_buffer_at);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (c == '>') {
                t->state = HTMLSTREAM_STATE_TEXT;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at,
                            t->val_buffer, t->val_buffer_at);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (c == '/') {
                //thrash
            } else if (isspace(c)) {
                t->state = HTMLSTREAM_STATE_OPENING_TAG_END;
                if (t->attr_cb) {
                    t->attr_cb(t->parg, t->tag_buffer, t->tag_buffer_at,
                            t->val_buffer, t->val_buffer_at);
                }
                t->tag_buffer_at = 0;
                t->val_buffer_at = 0;
            } else if (c == '\'' || c == '"') {
                t->state = HTMLSTREAM_STATE_ATTR_VAL_QUOTE;
                t->quote = c;
            } else {
                if (t->val_buffer_at < HTMLSTREAM_TAG_BUFFER_SIZE) {
                    t->val_buffer[t->val_buffer_at++] = tolower(c);
                }
            }
            break;
        case HTMLSTREAM_STATE_ATTR_VAL_QUOTE:
            if (c == '\\') {
                t->escaped = true;
            } else {
                if (c == t->quote && !t->escaped) {
                    t->state = HTMLSTREAM_STATE_ATTR_VAL;
                } else {
                    if (t->val_buffer_at < HTMLSTREAM_TAG_BUFFER_SIZE) {
                        t->val_buffer[t->val_buffer_at++] = tolower(c);
                    }
                }
                t->escaped = false;
            }
            break;
    }
}

//TODO find comments and shit
void htmlstream_feed(htmlstream_t *t, const char *data, int len)
{
    for (int i = 0; i < len; i++) {
        char c = data[i];
        switch (t->comment) {
            case 0:
                if (c == '<') {
                    t->comment = 1;
                    continue;
                }
                break;
            case 1:
                if (c == '!') {
                    t->comment = 2;
                    continue;
                }
                break;
            case 2:
                if (c == '-') {
                    t->comment = 3;
                    continue;
                }
                break;
            case 3:
                if (c == '-') {
                    t->comment = 4;
                    continue;
                }
                break;
            case 4:
                if (c == '-') {
                    t->comment = 5;
                } else {
                    t->comment = 4;
                }
                continue;
                break;
            case 5:
                if (c == '-') {
                    t->comment = 6;
                } else {
                    t->comment = 4;
                }
                continue;
            case 6:
                if (c == '>') {
                    t->comment = 0;
                } else {
                    t->comment = 4;
                }
                continue;
                break;
        }
        //was not comment
        const char *backfeed = "<!-";
        for (int i = 0; i < t->comment; i++) {
            htmlstream_internal_feed(t, backfeed[i]);
        }
        t->comment = 0;
        htmlstream_internal_feed(t, c);
    }
}


//test


#if 1

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
        printf("[PASS %d] text </%.*s> == </%s> \n", actual_cb_count, len, text, expected_val);
    } else {
        printf("[FAIL %d] text </%.*s> != </%s> \n", actual_cb_count, len, text, expected_val);
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


    TEST_TEXT("pew pew", "<lazer makes> pew pew </lazer>");


    if (expected_cb_count == actual_cb_count) {
        printf("[PASS] %d/%d passed\n", actual_cb_count, expected_cb_count);
    } else {
        printf("[FAIL] %d/%d missing \n", expected_cb_count - actual_cb_count, expected_cb_count);
        exit(3);
    }
}

#else

#include <unistd.h>
static int indent = 0;
void pretty_open_cb(void *parg, const char *tag, int len)
{
    ++indent;
    printf("%*s<%.*s>\n", indent, " ", len, tag);
}
void pretty_close_cb(void *parg, const char *tag, int len)
{
    --indent;
    printf("%*s</%.*s>\n", indent, " ", len, tag);
}

int main(int argc, char **argv)
{
    htmlstream_t t;
    htmlstream_init(&t);
    t.open_cb = pretty_open_cb;
    t.close_cb = pretty_close_cb;

    for (;;) {
        char buf[1024];
        int l = read(fileno(stdin), buf, 1024);
        if (l < 1)
            return 0;
        htmlstream_feed(&t, buf, l);
    }

}
#endif
