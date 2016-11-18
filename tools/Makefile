#DEF = a
#DEF = i
#DEF = hog
DEF = multiply

DEBIAN_DEVSYS = $(shell grep -q -s Debian /etc/dogtag; echo $$?)
DEBIAN = 0
NOT_DEBIAN = 1
DEVSYS = 2

ifeq ($(DEBIAN_DEVSYS),$(DEVSYS))
	CFLAGS += -DDEVSYS
else
endif

DIRS = . .. ../rx/CuteSDR
I = $(addprefix -I,$(DIRS))

all: $(DEF)

a: audio.c
	cc -o a audio.c

i: integrate.c
	cc -o i integrate.c

hog: hog.c
	cc $(CFLAGS) $(I) -lrt -o hog hog.c

multiply: multiply.c
	g++ $(CFLAGS) $(I) -o multiply multiply.c

run: $(DEF)
	./$(DEF)

clean:
	rm -f a i hog multiply
