CC = gcc

# -Wall -Wextra 
CFLAGS = -g -O2 -Iinclude -fsanitize=address
SRC = $(wildcard src/*.c)
OUT = app

LIBS = \
	-lX11 \
	-lXext \
	-lXcomposite \
	-lXdamage \
	-lXfixes \
	-lXrandr \
	-lXtst \
	-lpthread \
	-llz4 \
	-lsrt \
	-lm

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LIBS)

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
