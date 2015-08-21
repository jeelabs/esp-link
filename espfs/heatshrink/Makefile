PROJECT = heatshrink
#OPTIMIZE = -O0
#OPTIMIZE = -Os
OPTIMIZE = -O3
WARN = -Wall -Wextra -pedantic #-Werror
CFLAGS += -std=c99 -g ${WARN} ${OPTIMIZE}
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wmissing-declarations

# If libtheft is available, build additional property-based tests.
# Uncomment these to use it in test_heatshrink_dynamic.
#CFLAGS += -DHEATSHRINK_HAS_THEFT
#LDFLAGS += -ltheft

all:
	@echo "For tests, make test_heatshrink_dynamic (default) or change the"
	@echo "config.h to disable static memory and build test_heatshrink_static."
	@echo "For the standalone command-line tool, make heatshrink."

${PROJECT}: heatshrink.c

OBJS= 	heatshrink_encoder.o \
	heatshrink_decoder.o \

heatshrink: ${OBJS}
test_heatshrink_dynamic: ${OBJS} test_heatshrink_dynamic_theft.o
test_heatshrink_static: ${OBJS}

*.o: Makefile heatshrink_config.h

heatshrink_decoder.o: heatshrink_decoder.h heatshrink_common.h
heatshrink_encoder.o: heatshrink_encoder.h heatshrink_common.h

tags: TAGS

TAGS:
	etags *.[ch]

diagrams: dec_sm.png enc_sm.png

dec_sm.png: dec_sm.dot
	dot -o $@ -Tpng $<

enc_sm.png: enc_sm.dot
	dot -o $@ -Tpng $<

clean:
	rm -f ${PROJECT} test_heatshrink_{dynamic,static} *.o *.core {dec,enc}_sm.png TAGS
