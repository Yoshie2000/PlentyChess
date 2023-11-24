CC = g++
CFLAGS = -Wall -pedantic -Wextra -fcommon -pthread -O3 -fgnu89-inline

SOURCES = src/engine.cpp src/board.cpp src/move.cpp src/uci.cpp
OBJS = $(patsubst %.cpp,%.o, $(SOURCES))

all:	engine

%o:		%.cpp
		$(CC) $(CFLAGS) -c $<

engine:	$(OBJS)
		$(CC) $(CFLAGS) $^ -o $@

clean:	
		$(RM) src/*.o *~ engine