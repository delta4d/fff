#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>

#define PICK_VERSION "0.0.1"

#define MAX_LEN      128
#define MAX_CHOICES  1024
#define TTY          "/dev/tty"

typedef struct choice_t {
    char *str;
    int len;
    int score;
} choice_t;

typedef struct pattern_t {
    char str[MAX_LEN+1];
    int len;
} pattern_t;

pattern_t pattern;

char buf[MAX_LEN+1];

choice_t choices[MAX_CHOICES];
int choices_n;
int matches_n;
int select_n;

FILE *tty_in, *tty_out;
SCREEN *scr;
WINDOW *pattern_win, *choices_win;

bool alive = true;

int read_choices() {
    int n = 0;
    select_n = 0;
    while (fgets(buf, MAX_LEN, stdin)) {
        char *p = strchr(buf, '\n');
        if (p != NULL) *p = '\0';
        choices[n].str = (char *)strdup(buf);
        choices[n].len = strlen(buf);
        choices[n].score = 0;
        ++n;
    }
    return n;
}

int choice_cmp(const void *a, const void *b) {
    choice_t *pa = (choice_t *)a;
    choice_t *pb = (choice_t *)b;
    return pb->score - pa->score;
}

void pattern_match_choice(choice_t *choice) {
    pattern.str[pattern.len] = '\0';
    if (strstr(choice->str, pattern.str)) {
        choice->score = 1;
    } else {
        choice->score = 0;
    }
}

void pattern_match() {
    for (int i=0; i<choices_n; ++i) {
        pattern_match_choice(choices+i);
    }
    qsort(choices, choices_n, sizeof(choice_t), choice_cmp);
    for (matches_n=0; matches_n<choices_n&&choices[matches_n].score>0; ++matches_n);
    select_n = 0;
}

void pattern_update(int key) {
	if (key == KEY_BACKSPACE) {
		if (pattern.len > 0) --pattern.len;
		pattern_match();
	} else if (isprint(key) && pattern.len + 1 < MAX_LEN) {
        pattern.str[pattern.len++] = key;
		pattern_match();
    }
}

void display() {
	werase(choices_win);
    for (int i=0; i<matches_n; ++i) {
        if (i == select_n) wattron(choices_win, A_STANDOUT);
        mvwaddstr(choices_win, i, 0, choices[i].str);
        if (i == select_n) wattroff(choices_win, A_STANDOUT);
    }
    wrefresh(choices_win);

	werase(pattern_win);
    mvwaddstr(pattern_win, 0, 0, "> ");
    for (int i=0; i<pattern.len; ++i) {
        mvwaddch(pattern_win, 0, i+2, pattern.str[i]);
    }
    wmove(pattern_win, 0, pattern.len+2);
    wrefresh(pattern_win);
}

void deal_keypress() {
    int ch = wgetch(choices_win);
    mvwprintw(choices_win, 10, 0, "%s %d", keyname(ch), ch);
    switch (ch) {
        case 3:    // ^C
        case 17: { // ^Q
            alive = false;
            return;
         }
        case '\n': {
            fprintf(stdout, "%s", choices[select_n].str);
            alive = false;
            return;
        }
        case 14: // ^N
        case KEY_DOWN: {
            ++select_n;
            if (select_n == matches_n) select_n = 0; // cycling
            return;
        }
        case 16: // ^P
        case KEY_UP: {
            --select_n;
            if (select_n == -1) select_n = matches_n - 1; // cycling
            return;
         }
        default: {
            pattern_update(ch);
            break;
         }
    }
}

void init_curses() {
    tty_in = fopen(TTY, "r");
    tty_out = fopen(TTY, "w");
    scr = newterm(NULL, tty_out, tty_in);
    set_term(scr);

    pattern_win = newwin(1, COLS, 0, 0);
    choices_win = newwin(LINES-1, COLS, 1, 0);

    raw();
    noecho();
    keypad(pattern_win, TRUE);
    keypad(choices_win, TRUE);

    refresh();
}

void end_curses() {
    endwin();
}

int main() {
    choices_n = matches_n = read_choices();
    init_curses();
    while (alive) {
        display();
        deal_keypress();
    }
    end_curses();
    return 0;
}
