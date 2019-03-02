/*
 * mines.c - Minesweeper implemented with the ncurses library.
 *
 * Written by Ryan Antkowiak (antkowiak@gmail.com)
 *
 * March 2, 2019
 *
 * Copyright (c) 2019, All rights reserved.
 *
 */

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//
// The underlying data storage for each cell
//
typedef int8_t TCell;

//
// Cell Types
//
static TCell ERROR = INT8_MAX;
static TCell MINE = '*';
static TCell REVEAL = 'R';
static TCell FLAGGED = 'F';
static TCell WRONG_FLAG = 'X';
static TCell BLANK = 0;

//
// Generate a random integer in the range [start, end)
//
TCell random(const int start = 0, const int end = RAND_MAX)
{
    if (start < 0 || end < 1 || start >= end)
        return ERROR;

    int range = end - start;
    int r = rand() % range;
    r += start;

    if (r > ERROR)
        return ERROR;

    return (TCell) r;
}

//
// Structure to represent the minesweeper grid of cells, and their state
//
struct SBoard
{
private:

    // Current cursor position
    TCell curY;
    TCell curX;

    // Game state
    bool done;
    bool win;
    bool lose;
    TCell revealCount;
    TCell flagCount;

    // Board dimensions
    const TCell height;
    const TCell width;
    const TCell mines;

    // Cell states
    TCell * board = NULL;
    TCell * input_board = NULL;

    // Timestamp when the game is started
    struct timespec startTime;

    // Handles to the ncurses windows
    WINDOW * pScore;
    WINDOW * pField;

    // Unused default constructor
    SBoard() : height(0), width(0), mines(0) {}

public:

    //
    // Constructor for the minesweeper board
    //
    SBoard( const TCell height,
            const TCell width,
            const TCell mines,
            WINDOW * pScoreWin,
            WINDOW * pFieldWin)
    : height(height), width(width), mines(mines), pScore(pScoreWin), pField(pFieldWin)
    {
        curY = 0;
        curX = 0;

        init();
    }

    //
    // Initialize the minesweeper board and generate the mines
    //
    void init()
    {
        // Initialize the state variables
        done = false;
        win = false;
        lose = false;
        revealCount = 0;
        flagCount = 0;

        // Free/Allocate/Zero the memory of the mine board
        if (board != NULL)
        {
            free(board);
        }

        board = (TCell *) malloc(sizeof(TCell) * width * height);
        bzero(board, sizeof(TCell) * width * height);

        // Free/Allocate/Zero the memory of the player input board
        if (input_board != NULL)
        {
            free(input_board);
        }

        input_board = (TCell *) malloc(sizeof(TCell) * width * height);
        bzero(input_board, sizeof(TCell) * width * height);

        // Add the mines
        TCell minesAdded = 0;
        while (minesAdded < mines)
        {
            TCell y = random(0, height);
            TCell x = random(0, width);

            // Ensure we aren't placing a mine on a cell that already has one
            if (get(y, x) == 0)
            {
                set(y, x, MINE);
                ++minesAdded;
            }
        }

        // Calculate the number of neighboring mines for each cell
        for (TCell h = 0 ; h < height ; ++h)
        {
            for (TCell w = 0 ; w < width ; ++w)
            {
                if (get(h, w) != MINE)
                {
                    set(h, w, count_neighbors(h, w) + '0');
                }
            }
        }

        // Reset the start clock
        clock_gettime(CLOCK_MONOTONIC_RAW, &startTime);
    }

    //
    // Destructor. Free the memory.
    //
    ~SBoard()
    {
        if (board != NULL)
        {
            free(board);
        }

        if (input_board != NULL)
        {
            free(input_board);
        }
    }

    //
    // Gets the cell value of the mine board at index y,x
    //
    const TCell get(const TCell y, const TCell x)
    {
        if (is_valid(y, x))
        {
            return board[x * height + y];
        }

        return ERROR;
    }

    //
    // Sets the cell value of the mine board at index y,x
    //
    void set(const TCell y, const TCell x, const TCell val)
    {
        if (is_valid(y, x))
        {
            board[x * height + y] = val;
        }
    }

    //
    // Gets the cell value of the player input board at index y,x
    //
    const TCell geti(const TCell y, const TCell x)
    {
        if (is_valid(y, x))
        {
            return input_board[x * height + y];
        }

        return ERROR;
    }

    //
    // Sets the cell value of the player input board at index y,x
    //
    void seti(const TCell y, const TCell x, const TCell val)
    {
        if (is_valid(y, x))
        {
            input_board[x * height + y] = val;
        }
    }

    //
    // Reveals the cell at the current cursor position
    //
    void reveal()
    {
        // If this is the first reveal, make sure it is not a mine.
        while (revealCount == 0 && get(curY, curX) == MINE)
        {
            init();
        }

        // Reset the starting timer upon the first successful cell reveal
        if (revealCount == 0)
        {
            clock_gettime(CLOCK_MONOTONIC_RAW, &startTime);
        }

        // Call recursive reveal (to handle auto-reveal cells that
        // have zero neighboring mines.
        r_reveal(curY, curX);
    }

    //
    // Recursive reveal the cell at y,x and neighboring cells with zero mines
    //
    void r_reveal(const TCell y, const TCell x)
    {
        // No need to recurse if the game is done
        if (!is_done())
        {
            // Don't recursively look at cells that are flagged
            if (geti(y, x) == FLAGGED)
            {
                return;
            }

            // Reveal the cell
            if (geti(y, x) != REVEAL)
            {
                seti(y, x, REVEAL);
                ++revealCount;
            }

            // Check if the player hit a mine
            if (get(y, x) == MINE)
            {
                win = false;
                lose = true;
                done = true;

                return;
            }

            // Check if the player revealed all cells successfully
            if (revealCount >= max_reveal())
            {
                win = true;
                lose = false;
                done = true;
                return;
            }

            // Check if needing to recurse for cells with zero neighboring
            // mines
            if (get(y, x) == '0')
            {
                for (TCell h = y-1 ; h <= y+1 ; ++h)
                    for (TCell w = x-1 ; w <= x+1 ; ++w)
                        if (is_valid(h, w) && !(h==y && w==x) && (geti(h, w) != REVEAL))
                            r_reveal(h, w);
            }
        }
    }

    //
    // Returns the number of cells that must be successfully revealed in order
    // to win the game
    //
    TCell max_reveal()
    {
        return ((height * width) - mines);
    }

    //
    // Toggle the flag on the cell at the current cursor position
    //
    void flag()
    {
        if (geti(curY, curX) == BLANK)
        {
            // Toggle flag on
            seti(curY, curX, FLAGGED);
            ++flagCount;
        }
        else if (geti(curY, curX) == FLAGGED)
        {
            // Toggle flag off
            seti(curY, curX, BLANK);
            --flagCount;
        }
    }

    //
    // Count the number of neighboring cells that have a mine
    //
    TCell count_neighbors(const TCell y, const TCell x)
    {
        TCell count = 0;

        for (TCell h = y-1 ; h <= y+1 ; ++h)
            for (TCell w = x-1 ; w <= x+1 ; ++w)
                if (is_valid(h, w) && !(h==y && w==x))
                    if (get(h, w) == MINE)
                        ++count;

        return count;
    }

    // 
    // Check if the cell location at y,x is in the valid range
    //
    bool is_valid(TCell y, TCell x)
    {
        return (y >= 0 && y < height && x >= 0 && x < width);
    }

    //
    // Quit the game
    //
    void quit()
    {
        done = true;
    }

    //
    // Check if the game is done
    //
    bool is_done()
    {
        return done;
    }

    //
    // Move the cursor dy in the y direction and dx in the x direction
    //
    void move_cur(const TCell dy, const TCell dx)
    {
        TCell newY = dy + curY;
        TCell newX = dx + curX;

        if (is_valid(newY, newX))
        {
            curY = newY;
            curX = newX;
        }
    }

    //
    // Update the score window and the mine field window
    //
    void update()
    {
        update_score();
        update_field();
    }

    // Update the mine field window view
    //
    void update_field()
    {
        // Iterate through all the cells in the mine field
        for (TCell y = 0 ; y < height ; ++y)
        {
            for (TCell x = 0 ; x < width ; ++x)
            {
                // Grab the mine value and the player input value
                TCell val = get(y, x);
                TCell ival = geti(y, x);

                if (ival == FLAGGED)
                {
                    // Flagged cells
                    mvwaddch(pField, y, x, FLAGGED);
                }
                else if (ival == REVEAL)
                {
                    // Revealed cells with neighboring mines
                    if (val >= '1' && val <= '8')
                    {
                        if (has_colors())
                        {
                            wattron(pField, COLOR_PAIR(val - '0'));
                        }

                        mvwaddch(pField, y, x, val);

                        if (has_colors())
                        {
                            wattroff(pField, COLOR_PAIR(val - '0'));
                        }
                    }
                    else if (val == '0')
                    {
                        // Revealed cells with no neighboring mines
                        mvwaddch(pField, y, x, ' ');
                    }
                    else
                    {
                        // The mines hit
                        mvwaddch(pField, y, x, val);
                    }
                }
                else
                {
                    // Unrevealed cells
                    mvwaddch(pField, y, x, '.');
                }
            }
        }

        // If the player has lost, reveal the locations of all the mines
        if (lose)
        {
            // Iterate over all the cells in the mine field
            for (TCell h = 0 ; h < height ; ++h)
            {
                for (TCell w = 0 ; w < width ; ++w)
                {
                    // Grab the mine value and the player input value
                    TCell val = get(h, w);
                    TCell ival = geti(h, w);

                    // If the cell contains a mine
                    if (val == MINE)
                    {
                        // If the cell was not flagged by the player
                        if (ival != FLAGGED)
                        {
                            if (has_colors() && (curY == h) && (curX == w))
                            {
                                wattron(pField, COLOR_PAIR(3));
                            }

                            // Indicate to the player the location of the mine
                            mvwaddch(pField, h, w, MINE);

                            if (has_colors() && (curY == h) && (curX == w))
                            {
                                wattroff(pField, COLOR_PAIR(3));
                            }
                        }
                    }
                    
                    // Indicate an incorrectly placed flag
                    if (val != MINE && ival == FLAGGED)
                    {
                        mvwaddch(pField, h, w, WRONG_FLAG);
                    }
                }
            }
        }

        // Move the cursor back to the correct place and refresh the window
        wmove(pField, curY, curX);
        wrefresh(pField);
    }

    //
    // Update the score window view
    //
    void update_score()
    {
        // Grab the current time, and calculate the delta since the start of
        // the game
        struct timespec curTime;
        clock_gettime(CLOCK_MONOTONIC_RAW, &curTime);
        uint64_t delta_ms = ((curTime.tv_sec - startTime.tv_sec) * 1000000 +
                            (curTime.tv_nsec - startTime.tv_nsec) / 1000) / 1000;

        // If no cells have been revealed yet, don't show any time delta
        if (revealCount == 0)
        {
            delta_ms = 0;
        }

        // Print the scoreboard window
        mvwprintw(pScore, 1, 0, "         Minesweeper");
        mvwprintw(pScore, 3, 0, " [h] Move Left   [l] Move Right");
        mvwprintw(pScore, 4, 0, " [j] Move Down   [k] Move Up");
        mvwprintw(pScore, 5, 0, " [f] Flag Mine   [q] Quit");
        mvwprintw(pScore, 6, 0, " [space] Reveal");
        wmove(pScore, 8, 0);
        wclrtoeol(pScore);
        mvwprintw(pScore, 8, 0, "Flags: %2d / %2d  Status: %s", flagCount, mines, status());
        wmove(pScore, 9, 0);
        wclrtoeol(pScore);
        mvwprintw(pScore, 9, 0, "Time: %d ms", delta_ms);

        wrefresh(pScore);
    }

    // Return a string representation of the outcome of the game
    const char * status()
    {
        if (lose)
            return("Lose");
        if (win)
            return("Win");
        if (done)
            return("Aborted");

        return("Playing");
    }
};

//
// Play through the mine sweeper game
//
void minesweeper(const TCell height, const TCell width, const TCell mines)
{
    // Initialize ncurses
    initscr();

    // If the terminal supports colors, initialize color pairs
    if (has_colors())
    {
        start_color();
        init_pair(1, COLOR_BLUE, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(5, COLOR_RED, COLOR_BLACK);
        init_pair(6, COLOR_CYAN, COLOR_BLACK);
        init_pair(7, COLOR_WHITE, COLOR_BLACK);
        init_pair(8, COLOR_WHITE, COLOR_BLACK);
    }

    // Ncurses setings
    cbreak();
    noecho();

    // Ncurses windows for the scoreboard and mine field
    WINDOW * pScore = newwin(11, 31, 1, 1);
    WINDOW * pField = newwin(height, width, 12, 1);

    // Create the mine sweeper board and update the view
    SBoard board(height, width, mines, pScore, pField);
    board.update();

    // Set a timeout for input, so the timer/clock updates automatically
    nodelay(pField, true);
    wtimeout(pField, 1000);

    // Main Loop
    while (!board.is_done())
    {
        int key = wgetch(pField);
        switch (key)
        {
            case KEY_DOWN:
            case 'j':
                board.move_cur(1, 0);
                break;
            case KEY_UP:
            case 'k':
                board.move_cur(-1, 0);
                break;
            case KEY_LEFT:
            case 'h':
                board.move_cur(0, -1);
                break;
            case KEY_RIGHT:
            case 'l':
                board.move_cur(0, 1);
                break;
            case ' ':
                board.reveal();
                break;
            case 'f':
                board.flag();
                break;
            case 'q':
                board.quit();
                break;
        }

        // Update the board view
        board.update();
    }

    // Ncurses end
    endwin();
}

//
// Main function. Starts a minesweeper game in beginner mode by default.
//
int main(int argc, char * argv[])
{
    // Command line option handling
    bool bFlag = false;
    bool iFlag = false;
    bool eFlag = false;
    bool flagErr = false;
    int flagCount = 0;
    int opt;

    while ((opt = getopt(argc, argv, "bie")) != -1)
    {
        switch (opt)
        {
            // Beginner mode
            case 'b':
                bFlag = true;
                ++flagCount;
                break;
            // Intermediate mode
            case 'i':
                iFlag = true;
                ++flagCount;
                break;
            // Expert mode
            case 'e':
                eFlag = true;
                ++flagCount;
                break;
            default:
                flagErr = true;
                break;
        }
    }

    // Check for invalid command line options
    if (flagErr || flagCount > 1)
    {
        fprintf(stderr, "Usage: %s [-b|-i|-e]\n", argv[0]);
        fprintf(stderr, "    -b    Beginner       8 x 8  grid with 10 mines\n");
        fprintf(stderr, "    -i    Intermediate  16 x 16 grid with 40 mines\n");
        fprintf(stderr, "    -e    Expert        16 x 30 grid with 99 mines\n");
        return EXIT_FAILURE;
    }

    // Seed randomizer with current time
    srand(time(0));

    if (bFlag || flagCount == 0)
    {
        // Play beginner mode
        minesweeper(8, 8, 10);
    }
    else if (iFlag)
    {
        // Play intermediate mode
        minesweeper(16, 16, 40);
    }
    else if (eFlag)
    {
        // Play expert mode
        minesweeper(16, 30, 99);
    }

    return EXIT_SUCCESS;
}


