CC = gcc

# -Wall -Wextra 
CFLAGS = -g -O0 -Iinclude
SRC = $(wildcard src/*.c)
OUT = app

LIBS = \
	-lX11 \
	-lXcomposite \
	-lXdamage \
	-lXfixes \
	-lXtst

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
