#define _GNU_SOURCE

#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <ncurses.h>

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define FFF_VERSION "0.0.2"
#define MAX_LEN     80
#define MAX_LEN_S   TO_STRING(MAX_LEN)
#define PROMPT      "> "
#define PROMPT_LEN  2
#define TTY         "/dev/tty"

#define CTRL(c) ((c) & 0x1f)

#define EXIT_IF_NULL(p, msg) do {       \
    __exit_if_null((void *)(p), (msg)); \
} while (0)


const char *PATTERN_LENGTH_EXCEED = "maximum pattern length is "TO_STRING(MAX_LEN);

// exit code
enum error_code {
    EC_USAGE = 64,
    EC_NULL,
};

typedef struct choice_t {
    char *str;
    int id;
    int len;
    int score;
} choice_t;

typedef struct choices_t {
    choice_t **list;
    int len;
    int size;
    int start;         // start of the matching item to show. update while page scroll
    int matched_num;
    int selected_idx;
} choices_t;

typedef struct pattern_t {
    char *str;
    int len;
    int curs;
} pattern_t;

bool with_selection = false;
bool alive = true;
bool any_flash_msg = false;
char *flash_msg;
char buffer[MAX_LEN+1];

choices_t *choices;
pattern_t *pattern;

// ------------------------------
// prompt window | pattern window
// ------------------------------
// match status window (22/33)
// ------------------------------
// choices window
// choice 1
// choice 2
// ...
// ------------------------------
WINDOW *prompts_w;
WINDOW *pattern_w;
WINDOW *mstatus_w;
WINDOW *choices_w;


void __exit_if_null(void *p, char *msg) {
    if (p == NULL) {
        fprintf(stderr, "%s\n", msg);
        exit(EC_NULL);
    }
}

void help() {
    fprintf(stdout, "FFF -- Fuzzy File Finder\n");
    fprintf(stdout, "reads lists from stdin, search while typing\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Examples   ls | fff\n");
    fprintf(stdout, "           vim $(find -type f | fff)\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Shortcut   C-C    quit without select\n");
    fprintf(stdout, "           ENTER  quit with select\n");
    fprintf(stdout, "           C-N    next option\n");
    fprintf(stdout, "           C-P    previouse option\n");
}

pattern_t *pattern_new() {
    pattern_t *p = (pattern_t *)malloc(sizeof(pattern_t));
    EXIT_IF_NULL(p, "malloc pattern failed");

    p->str  = (char *)malloc(MAX_LEN * sizeof(char));
    p->len = 0;
    p->curs = 0;

    return p;
}

choice_t *choice_new(char *s, int id) {
    choice_t *p = (choice_t *)malloc(sizeof(choice_t));
    EXIT_IF_NULL(p, "malloc choice_t failed");

    p->str = strdup(s);
    EXIT_IF_NULL(p->str, "strdup failed");
    p->len = strlen(s);
    p->score = 0;
    p->id = id;

    return p;
}

choices_t *choices_new() {
    choices_t *p = (choices_t *)malloc(sizeof(choices_t));
    EXIT_IF_NULL(p, "malloc choices_t failed");

    p->list = NULL;
    p->len = 0;
    p->size = 0;
    p->matched_num = 0;
    p->selected_idx = 0;

    return p;
}

void choices_realloc(choices_t *cs) {
    int new_size = cs->size == 0 ? 1 : 2 * cs->size;

    choice_t **new_list = (choice_t **)malloc(new_size * sizeof(choice_t *));
    EXIT_IF_NULL(new_list, "malloc choices list failed");

    for (int i=0; i<cs->size; ++i) new_list[i] = cs->list[i];
    free(cs->list);

    cs->list = new_list;
    cs->size = new_size;
}

void choices_add(choices_t *cs, choice_t *c) {
    if (cs->len == cs->size) choices_realloc(cs);
    cs->list[cs->len++] = c;
}

void choices_read(choices_t *cs) {
    for (int id=0; fgets(buffer, MAX_LEN, stdin); ++id) {
        char *cp = strchr(buffer, '\n');
        if (cp != NULL) *cp = '\0';
        choice_t *c = choice_new(buffer, id);
        choices_add(cs, c);
    }
    choices->matched_num = choices->len;
}

void draw_prompts() {
    werase(prompts_w);
    waddstr(prompts_w, PROMPT);
    wrefresh(prompts_w);
}

void draw_pattern() {
    werase(pattern_w);
    waddstr(pattern_w, pattern->str);
    wmove(pattern_w, 0, pattern->curs);
    wrefresh(pattern_w);
}

void draw_mstatus() {
    werase(mstatus_w);
    wprintw(mstatus_w, "%d/%d", choices->matched_num, choices->len);
    if (any_flash_msg) wprintw(mstatus_w, " error: %s", flash_msg);
    wrefresh(mstatus_w);
}

int get_window_height(WINDOW *w) {
    int height, width;

    getmaxyx(choices_w, height, width);
    ++width; // dirty hack, make unused warning go away

    return height;
}

void choices_get_show_range(choices_t *cs, int *_st, int *_ed) {
    int h = get_window_height(choices_w);
    int st = cs->start;
    if (st > cs->selected_idx) st = cs->selected_idx;
    if (cs->selected_idx - st + 1 > h) st = cs->selected_idx - h + 1;
    cs->start = st;
    *_st = st;
    *_ed = st + h;
    if (*_ed > cs->matched_num) *_ed = cs->matched_num;
}

void draw_choice(int y, int x, choice_t *c) {
    for (char *cp=c->str, *pp=pattern->str; *cp; ++cp) {
        if (tolower(*pp) == tolower(*cp)) {
            wattron(choices_w, COLOR_PAIR(1));
            mvwaddch(choices_w, y, x++, *cp);
            wattroff(choices_w, COLOR_PAIR(1));
            if (*pp) ++pp;
        } else {
            mvwaddch(choices_w, y, x++, *cp);
        }
    }
}

void draw_choices() {
    werase(choices_w);

    int st, ed;
    choices_get_show_range(choices, &st, &ed); // deal with page scroll

    // mvwprintw(choices_w, 0, 20, "%d %d", st, ed);
    // for (int i=0; i<choices->len; ++i) {
    //     mvwprintw(choices_w, i-st, 0, "%s %d", choices->list[i]->str, choices->list[i]->score);
    // }

    for (int i=st; i<ed; ++i) {
        if (i == choices->selected_idx) wattron(choices_w, A_STANDOUT);
        draw_choice(i-st, 0, choices->list[i]);
        // mvwaddstr(choices_w, i-st, 0, choices->list[i]->str);
        if (i == choices->selected_idx) wattroff(choices_w, A_STANDOUT);
    }
    wrefresh(choices_w);
}

void draw() {
    draw_prompts();
    draw_mstatus();
    draw_choices();
    draw_pattern(); // pattern must be the last to update
}

void choices_up(choices_t *cs) {
    if (cs->selected_idx > 0) cs->selected_idx--;
}

void choices_down(choices_t *cs) {
    if (cs->selected_idx + 1 < cs->matched_num) cs->selected_idx++;
}

bool pattern_update(pattern_t *p, int key) {
    switch (key) {
        case KEY_BACKSPACE: {
            if (p->curs > 0) {
                for (int i=p->curs; i<p->len; ++i) p->str[i-1] = p->str[i];
                p->curs--;
                p->len--;
                p->str[p->len] = '\0';
                return true;
            }
            return false;
        }
        case CTRL('b'):
        case KEY_LEFT: {
            if (p->curs > 0) p->curs--;
            return false;
        }
        case CTRL('f'):
        case KEY_RIGHT: {
            if (p->curs + 1 < p->len) p->curs++;
            return false;
        }
        case CTRL('u'): {
            p->curs = 0;
            p->len = 0;
            p->str[0] = '\0';
            return true;
        }
        case CTRL('a'): {
            p->curs = 0;
            return false;
        }
        case CTRL('e'): {
            p->curs = p->len;
            return false;
        }
        default: break;
    }
    if (!isprint(key)) return false;
    if (p->len > MAX_LEN) {
        any_flash_msg = true;
        flash_msg = (char *)PATTERN_LENGTH_EXCEED;
        return false;
    }
    for (int i=p->len; i>p->curs; --i) p->str[i] = p->str[i-1];
    p->str[p->curs++] = key;
    p->len++;
    p->str[p->len] = '\0';
    return true;
}

int choice_cmp(const void *a, const void *b) {
    choice_t **pa = (choice_t **)a;
    choice_t **pb = (choice_t **)b;

    if ((*pa)->score != (*pb)->score) return (*pb)->score - (*pa)->score;
    return (*pa)->id - (*pb)->id;
}

void pattern_match_single(choice_t *c, pattern_t *p) {
    char *cp = c->str;
    char *pp = p->str;

    for (; *cp && *pp; ++cp) {
        if (tolower(*pp) == tolower(*cp) && *pp) ++pp;
    }

    if (*pp) c->score = INT_MIN;
    else c->score = p->len - c->len;
}

void choices_update(choices_t *cs) {
    qsort(cs->list, cs->len, sizeof(choice_t *), choice_cmp);
    int i = 0;
    while (i < cs->len && cs->list[i]->score > INT_MIN) ++i;
    cs->matched_num = i;
}

void pattern_match(choices_t *cs, pattern_t *p) {
    for (int i=0; i<cs->len; ++i) {
        pattern_match_single(cs->list[i], p);
    }
    choices_update(cs);
}

void keypress() {
    any_flash_msg = false;
    int ch = wgetch(pattern_w);
    switch (ch) {
        case CTRL('c'):
        case CTRL('q'): {
            alive = false;
            with_selection = false;
            break;
        }
        // ENTER
        case CTRL('m'): {
            alive = false;
            with_selection = true;
            break;
        }
        case CTRL('j'):
        case CTRL('n'):
        case KEY_DOWN: {
            choices_down(choices);
            break;
        }
        case CTRL('k'):
        case CTRL('p'):
        case KEY_UP: {
            choices_up(choices);
            break;
        }
        default: {
            if (pattern_update(pattern, ch)) {
                pattern_match(choices, pattern);
            }
        }
    }
}

void fff_init() {
    choices = choices_new();
    choices_read(choices);
    pattern = pattern_new();

    // for (int i=0; i<choices->len; ++i) puts(choices->list[i]->str);

    FILE *tty_i = fopen(TTY, "r");
    FILE *tty_o = fopen(TTY, "w");
    EXIT_IF_NULL(tty_i, "open "TTY" for read error");
    EXIT_IF_NULL(tty_o, "open "TTY" for write error");

    SCREEN *scr = newterm(NULL, tty_o, tty_i);
    set_term(scr);

    prompts_w = newwin(1, PROMPT_LEN, 0, 0);
    pattern_w = newwin(1, COLS-PROMPT_LEN, 0, PROMPT_LEN);
    mstatus_w = newwin(1, COLS, 1, 0);
    choices_w = newwin(LINES-2, COLS, 2, 0);

    raw();
    nonl();
    noecho();

    keypad(prompts_w, TRUE);
    keypad(pattern_w, TRUE);
    keypad(mstatus_w, TRUE);
    keypad(choices_w, TRUE);

    start_color();
    use_default_colors();
    init_pair(1, COLOR_YELLOW, -1);
}

void fff_main() {
    while (alive) {
        draw();
        keypress();
    }
}

void fff_done() {
    delwin(prompts_w);
    delwin(pattern_w);
    delwin(choices_w);

    endwin();

    if (with_selection && choices->selected_idx < choices->len) {
        fprintf(stdout, "%s\n", choices->list[choices->selected_idx]->str);
    }

    for (int i=0; i<choices->len; ++i) free(choices->list[i]);
    free(choices->list);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        help();
        exit(EC_USAGE);
    }

    fff_init();
    fff_main();
    fff_done();

    return 0;
}
