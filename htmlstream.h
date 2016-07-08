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


static void htmlstream_init(htmlstream_t *t)
{
    memset(t, 0, sizeof(htmlstream_t));
}

static char *htmlstream_trimwhitespace(char *in, int *len)
{
    int nustart = 0;
    int nulen   = *len;
    for (int i = 0; i < *len ; i++) {
        if (isspace(in[i])) {
            ++nustart;
            --nulen;
        } else {
            break;
        }
    }
    for (int i = (*len - 1); i > 0; i--) {
        if (isspace(in[i])) {
            --nulen;
        } else {
            break;
        }
    }
    if (nulen < 0) {
        *len = 0;
    } else {
        *len = nulen;
    }
    return in + nustart;
}

static void htmlstream_internal_feed(htmlstream_t *t, const char c)
{
    switch (t->state) {
        case HTMLSTREAM_STATE_TEXT:
            if (c == '<') {
                t->state = HTMLSTREAM_STATE_TAG_START;
                //trim

                if (t->text_cb) {
                    char *nu = htmlstream_trimwhitespace(t->val_buffer, &t->val_buffer_at);
                    if (t->val_buffer_at) {
                        t->text_cb(t->parg, nu, t->val_buffer_at);
                    }
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
static void htmlstream_feed(htmlstream_t *t, const char *data, int len)
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
