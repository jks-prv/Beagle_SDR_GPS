#DEF = a
DEF = i

all: $(DEF)

a: audio.c
	cc -o a audio.c

i: integrate.c
	cc -o i integrate.c

run: $(DEF)
	./$(DEF)

clean:
	rm -f a i
