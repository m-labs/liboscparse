CC=lm32-rtems4.11-gcc
LD=lm32-rtems4.11-gcc
AR=lm32-rtems4.11-ar

CFLAGS=-O9 -Wall -mbarrel-shift-enabled -mmultiply-enabled -mdivide-enabled -msign-extend-enabled -I$(RTEMS_MAKEFILE_PATH)/lib/include -I.

OBJS=blob.o pattern_match.o timetag.o method.o message.o server.o

all: liblop.a

liblop.a: $(OBJS)
	$(AR) clq liblop.a $(OBJS)

clean:
	rm -f liblop.a $(OBJS)

.PHONY: clean
