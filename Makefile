CC=lm32-rtems4.11-gcc
LD=lm32-rtems4.11-gcc
AR=lm32-rtems4.11-ar

CFLAGS=-O9 -Wall -Wstrict-prototypes -mbarrel-shift-enabled -mmultiply-enabled -mdivide-enabled -msign-extend-enabled -I$(RTEMS_MAKEFILE_PATH)/lib/include -I.

OBJS=blob.o pattern_match.o timetag.o method.o message.o server.o

all: liblop.a

liblop.a: $(OBJS)
	$(AR) clr liblop.a $(OBJS)

clean:
	rm -f liblop.a $(OBJS)

install: liblop.a
	test -n "$(RTEMS_MAKEFILE_PATH)"
	cp liblop.a $(RTEMS_MAKEFILE_PATH)/lib
	mkdir -p $(RTEMS_MAKEFILE_PATH)/lib/include/lop
	cp lop/* $(RTEMS_MAKEFILE_PATH)/lib/include/lop

.PHONY: clean install
