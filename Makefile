CC = gcc

CFLAGS = -Wall -Wextra -g -Iinclude


TARGET = bin/edf_vd_sim


SRCS = src/main.c src/edf_vd.c src/min_heap.c src/utils.c


OBJS = $(SRCS:.c=.o)


all: $(TARGET)


$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Build successful: $(TARGET)"


src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f src/*.o $(TARGET)
	@echo "Cleaned up build files."


run: $(TARGET)
	./$(TARGET) inputs/taskset1.txt

.PHONY: all clean run