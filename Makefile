
TARGET  := loki_demos
VERSION := \"1.0e\"
OBJS	:= loki_demos.o loki_launch.o
CFLAGS  := -g -Wall -DVERSION=$(VERSION)
CFLAGS  += $(shell sdl-config --cflags)
LFLAGS  := -lSDL_image -lpng -lz -ljpeg -lSDL_mixer
LFLAGS  += $(shell sdl-config --static-libs)
ARCH    := $(shell sh print_arch)
ifeq ($(ARCH), alpha)
CFLAGS  += -mcpu=ev4 -Wa,-mall
endif
CDBASE := /loki/demos/loki_demos-$(shell echo $(VERSION))
export CDBASE
INSTALL := $(CDBASE)/bin/$(ARCH)/$(TARGET)
DEMO_CONFIG := demo_config

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LFLAGS) -static

install: $(TARGET)
	@echo "$(TARGET) -> $(INSTALL)"
	@cp -p $(TARGET) $(INSTALL)
	@strip $(INSTALL)
	-brandelf -t $(shell uname -s) $(INSTALL)
	make -C $(DEMO_CONFIG) $@

clean:
	rm -f $(TARGET) *.o
	make -C $(DEMO_CONFIG) $@
