CC = gcc

# -Wall -Wextra 
CFLAGS = -g -O0 -Iinclude
SRC = $(wildcard src/*.c)
OUT = app

LIBS = \
	-lX11 \
	-lXext \
	-lXcomposite \
	-lXdamage \
	-lXfixes \
	-lXtst \
	-lpthread

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
