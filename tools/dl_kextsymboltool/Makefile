CC = gcc
TARGET = kextsymtool
SRCS = $(wildcard *.c)

all : $(TARGET)

$(TARGET) : $(SRCS)
	$(CC) $(SRCS) -o $(TARGET)

clean :
	rm -rf $(TARGET)
