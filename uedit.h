#ifndef UEDIT_H
#define UEDIT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    #define GETCH() _getch()
#else
    #include <termios.h>
    #include <unistd.h>
    #define GETCH() getchar()
#endif

#define MAX_LINE 1024
#define CTRL_KEY(k) ((k) & 0x1f)

static char last_cmd[MAX_LINE] = {0};

#define the_line(n) ((n)<(MAX_LINE)?(n):(MAX_LINE))

#ifndef _WIN32
static struct termios orig_termios;
static void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
static void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
#else
static DWORD orig_mode;
static void enable_raw_mode() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hIn, &orig_mode);
    SetConsoleMode(hIn, orig_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
    DWORD outMode = 0;
    GetConsoleMode(hOut, &outMode);
    SetConsoleMode(hOut, outMode | 0x0004); // ENABLE_VIRTUAL_TERMINAL_PROCESSING
}
static void disable_raw_mode() { SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), orig_mode); }
#endif

static void refresh_line(const char *prompt, const char *buf, int len, int cur) {
    int p_len = (int)strlen(prompt);
    // \r moves to start, \033[K clears from cursor to end of line
    printf("\r\033[K%s%s", prompt, buf);
    
    // Move cursor back to the logical 'cur' position from the start of the line
    printf("\r");
    for (int i = 0; i < (cur + p_len); i++) {
        printf("\033[C");
    }
    fflush(stdout);
}

int uedit(const char *prompt, char *buf, int max_line) {
    int r = 0;
    int len = 0, cur = 0, c;
    memset(buf, 0, the_line(max_line));
    
    // Initial prompt print
    printf("%s", prompt);
    fflush(stdout);
    
    enable_raw_mode();

    while (1) {
        c = GETCH();

        // 1. Navigation & Basic Control
        if (c == CTRL_KEY('a')) { cur = 0; } 
        else if (c == CTRL_KEY('e')) { cur = len; }
        else if (c == '\n' || c == '\r') {
            buf[len] = '\0';
            putchar('\n');
            if (len > 0) strcpy(last_cmd, buf);
            break;
        }
        // 2. Backspace
        else if (c == 127 || c == 8) {
            if (cur > 0) {
                memmove(&buf[cur - 1], &buf[cur], len - cur);
                len--; cur--;
                buf[len] = '\0';
            }
        }
        // 3. Escape Sequences (Unix/ANSI)
        else if (c == 27) {
            c = GETCH();
            if (c == '[') {
                c = GETCH();
                if (c == 'D' && cur > 0) { cur--; }
                else if (c == 'C' && cur < len) { cur++; }
                else if (c == 'A' && last_cmd[0]) {
                    strcpy(buf, last_cmd);
                    len = (int)strlen(buf); cur = len;
                }
                else if (c == '3') { // Delete key
                    GETCH(); // consume '~'
                    if (cur < len) {
                        memmove(&buf[cur], &buf[cur + 1], len - cur - 1);
                        len--; buf[len] = '\0';
                    }
                }
            }
        }
#ifdef _WIN32
        // 4. Windows Extended Keys
        else if (c == 0 || c == 0xE0) {
            c = GETCH();
            if (c == 75 && cur > 0) { cur--; }      // Left
            else if (c == 77 && cur < len) { cur++; } // Right
            else if (c == 71) { cur = 0; }           // Home
            else if (c == 79) { cur = len; }         // End
            else if (c == 83 && cur < len) {        // Delete
                memmove(&buf[cur], &buf[cur + 1], len - cur - 1);
                len--; buf[len] = '\0';
            }
            else if (c == 72 && last_cmd[0]) {      // Up
                strcpy(buf, last_cmd);
                len = (int)strlen(buf); cur = len;
            }
        }
#endif
        // 5. Normal Character Insertion
        else if (c >= 32 && c <= 126 && len < the_line(max_line) - 1) {
            memmove(&buf[cur + 1], &buf[cur], len - cur);
            buf[cur] = (char)c;
            len++; cur++;
        } else if (c == 4) { // eol at least on linux
          r = -1;
          break;
        }

        refresh_line(prompt, buf, len, cur);
    }
    disable_raw_mode();
    return r;
}

#endif
