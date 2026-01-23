
TARGET  := demo_config

OBJS    := demo_config$(GTK).o

CFLAGS  ?= -g -O2 -Wall
ARCH    := $(shell sh ../print_arch)
LIBC    := $(shell sh ../print_libc)
ifeq ($(ARCH), alpha)
CFLAGS  += -mcpu=ev4 -Wa,-mall
endif
ifeq ($(GTK), )
CFLAGS += $(shell gtk-config --cflags) $(shell libglade-config --cflags)
CFLAGS += $(shell xml-config --cflags)
LFLAGS += -Wl,-Bstatic
LFLAGS += -L$(shell libglade-config --prefix)
LFLAGS +=  -lglade
LFLAGS += -L$(shell gtk-config --prefix)
LFLAGS +=  -lgtk -lgdk -rdynamic -lgmodule -lglib 
LFLAGS += -L$(shell xml-config --prefix)
LFLAGS += -lxml -lz
LFLAGS += -Wl,-Bdynamic
LFLAGS += -L/usr/X11R6/lib -lXi -lXext -lX11 -lm -ldl
else
CFLAGS += $(shell pkg-config gtk4 --cflags)
LFLAGS += $(shell pkg-config gtk4 --libs)
LFLAGS += $(LDFLAGS)
endif

INSTALL := $(CDBASE)/bin/$(ARCH)/$(LIBC)/$(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LFLAGS)

install: $(TARGET)
	@echo "$(TARGET) -> $(INSTALL)"
	@cp -p $(TARGET) $(INSTALL)
	@strip $(INSTALL)

clean:
	rm -f *.o *.bak core $(TARGET)
