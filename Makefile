minesweeper : minesweeper.c
	g++ minesweeper.c -lm -lncurses -o minesweeper

clean :
	\rm -f minesweeper

