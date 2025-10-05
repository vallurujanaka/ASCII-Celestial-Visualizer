#include "term.h"

#include <curses.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <stdio.h>
#include <windows.h>
extern BOOL WINAPI GetCurrentConsoleFont(HANDLE hConsoleOutput, BOOL bMaximumWindow, PCONSOLE_FONT_INFO lpConsoleCurrentFont);
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

void ncurses_init(bool color)
{
    initscr();
    clear();
    noecho();    // Input characters aren't echoed
    cbreak();    // Disable line buffering
    curs_set(0); // Make cursor invisible
    timeout(0);  // Non-blocking read for getch

    // Set the console output code page to UTF-8 on Windows
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Initialize colors
    if (color)
    {
        if (!has_colors())
        {
            printf("Your terminal does not support colors");
            exit(EXIT_FAILURE);
        }

        start_color();
        use_default_colors(); // Use terminal colors (fg and bg for pair 0)

        // Colors with default backgrounds
        init_pair(1, COLOR_BLACK, -1);
        init_pair(2, COLOR_RED, -1);
        init_pair(3, COLOR_GREEN, -1);
        init_pair(4, COLOR_YELLOW, -1);
        init_pair(5, COLOR_BLUE, -1);
        init_pair(6, COLOR_MAGENTA, -1);
        init_pair(7, COLOR_CYAN, -1);
        init_pair(8, COLOR_WHITE, -1);
    }
}

void ncurses_kill(void)
{
    endwin();
}

void win_resize_square(WINDOW *win, float aspect)
{
    if (COLS < LINES * aspect)
    {
        wresize(win, COLS / aspect, COLS);
    }
    else
    {
        wresize(win, LINES, LINES * aspect);
    }
}

void wrectangle(WINDOW *win, int ya, int xa, int yb, int xb)
{
    mvwhline(win, ya, xa, 0, xb - xa);
    mvwhline(win, yb, xa, 0, xb - xa);
    mvwvline(win, ya, xa, 0, yb - ya);
    mvwvline(win, ya, xb, 0, yb - ya);
    mvwaddch(win, ya, xa, ACS_ULCORNER);
    mvwaddch(win, yb, xa, ACS_LLCORNER);
    mvwaddch(win, ya, xb, ACS_URCORNER);
    mvwaddch(win, yb, xb, ACS_LRCORNER);
}

void win_resize_full(WINDOW *win)
{
    wresize(win, LINES, COLS);
}

void win_position_center(WINDOW *win)
{
    int height, width;
    getmaxyx(win, height, width);
    int maxy = height - 1;
    int maxx = width - 1;

    int center_y = (LINES - maxy) / 2;
    int center_x = (COLS - maxx) / 2;

    mvwin(win, center_y, center_x);
}

void term_size(int *y, int *x)
{
#if defined(_WIN32)

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int columns, rows;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    *x = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else

    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    *y = (int)ws.ws_row;
    *x = (int)ws.ws_col;

#endif // _WIN32
}

bool stdout_directed_to_console(void)
{
#if defined(_WIN32)

    // Hacky way to check if stdout is directed to a console
    // https://stackoverflow.com/questions/2087775/how-do-i-detect-when-output-is-being-redirected
    fpos_t pos;
    fgetpos(stdout, &pos);
    return (pos == -1);

#else

    return (isatty(fileno(stdout)) != 0);

#endif // _WIN32
}

float get_cell_aspect_ratio(void)
{
    float default_height = 2;

    // Attempt to get aspect ratio only if stdout writing to console
    if (stdout_directed_to_console())
    {
#if defined(_WIN32)

        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_FONT_INFO cfi;
        GetCurrentConsoleFont(handle, false, &cfi);
        float cell_width = cfi.dwFontSize.X;
        float cell_height = cfi.dwFontSize.Y;

        return cell_height / cell_width;
#else

        struct winsize ws;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

        // In case we can't get pixel size of terminal (inconsistent support)
        if (ws.ws_ypixel == 0 || ws.ws_xpixel == 0)
        {
            return default_height;
        }

        float cell_height = (float)ws.ws_ypixel / ws.ws_row;
        float cell_width = (float)ws.ws_xpixel / ws.ws_col;

        return cell_height / cell_width;

#endif // _WIN32
    }

    return default_height;
}

#define MAX_STR_LEN 2048
void mvwaddstr_truncate(WINDOW *win, int y, int x, const char *str)
{
    // Remaining space on the current line
    int max_x = getmaxx(win);
    int space_left = max_x - x;

    // Don't write beyond the line
    if (space_left > 0)
    {
        // Truncate if necessary
        char truncated[MAX_STR_LEN];
        strncpy(truncated, str, space_left);
        truncated[space_left] = '\0';
        mvwaddstr(win, y, x, truncated);
    }
}

#ifdef _WIN32

// Greg Spears at Stackoverflow.com
// https://stackoverflow.com/questions/78082999/check-for-terminal-resizing-using-ncurses-under-windows

static HANDLE hConOut = NULL;

/*------------------------------------------------------------

 getConsoleOutputHandle()

 There are simpler ways to get the console handle, but they
 arguably lack precision.

*------------------------------------------------------------*/
HANDLE getConsoleOutputHandle(void)
{
    SECURITY_ATTRIBUTES sa;

    if (!hConOut)
    {
        /* First call -- get the window handle one time and save it*/
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        /* Using CreateFile we get the true console handle", avoiding any redirection.*/
        hConOut = CreateFile(TEXT("CONOUT$"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                             OPEN_EXISTING, (DWORD)0, (HANDLE)0);
    }
    if (!hConOut) /* actually, this is a bad error, let your app handle the error as needed*/
    {
        printf("getConsoleOutputHandle(): failed to get Console Window Handle\n");
        return NULL;
    }
    return hConOut;
}

/*----------------------------------------------------------------------------------------

    check_console_window_resize_event()

    Params: COORD to return new window size
    Returns: TRUE if the console window has changed size.  FALSE if not.

    USAGE: Best practice is to call the function repeatedly from your main application
    loop.   Preferably a place where the function can be called several times per second
    throughout the program's run time.

    DATE:           Reason:
    2022.10.05      Created -- placed in public domain
*----------------------------------------------------------------------------------------*/
int check_console_window_resize_event(COORD *info)
{
    static short old_screen_w = 0, old_screen_h = 0;
    /* variables declared static hold their value between function calls.*/
    short current_screen_w, current_screen_h;
    int window_resized = FALSE;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    hConOut = getConsoleOutputHandle();
    if (!hConOut)
        return FALSE;

    if (!GetConsoleScreenBufferInfo(getConsoleOutputHandle(), &csbi))
    {
        // printf("check_console_window_resize_event(): GetConsoleScreenBufferInfo() FAILED!!  %s\n", __FILE__);
        return FALSE;
    }
    current_screen_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    current_screen_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    if (!old_screen_w && !old_screen_h)
    {
        /* Execution comes here if this is first time this function is called.
        ** Initialize the static variables and bail...*/
        old_screen_w = current_screen_w;
        old_screen_h = current_screen_h;
        return FALSE;
    }

    /* At last the real work of this function can be realized...*/
    if (current_screen_w != old_screen_w || current_screen_h != old_screen_h)
    {
        old_screen_w = current_screen_w;
        old_screen_h = current_screen_h;
        window_resized = TRUE;
        info->X = current_screen_w;
        info->Y = current_screen_h;
        // printf("check_console_window_resize_event(): new screenwidth:  %d new screenheight:  %d", current_screen_w,
        // current_screen_h);
    }
    return window_resized;
}

#endif // _WIN32
