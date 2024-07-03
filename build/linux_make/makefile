#
# Project files
#
mkfile_path := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
inc_dir = $(abspath $(join $(mkfile_path), /../../include))
bin_dir = $(abspath $(join $(mkfile_path), /../../bin))
obj_dir = $(abspath $(join $(mkfile_path), /../../obj))
src_dir = $(abspath $(join $(mkfile_path), /../../src))
includes := $(wildcard $(join $(inc_dir), /*.h) $(join $(inc_dir), /processors/*.h))

vpath %.c $(src_dir)

SRCS = main.c cJSON.c processors/ldtk_to_map.c processors/png_to_png.c
OBJS = $(SRCS:.c=.o)
EXE  = c_content_processor

#
# Compiler flags
#
CC     = gcc
#CFLAGS = -std=c99 -Wall -Werror -Wextra -I ../../include
CFLAGS = -std=c99 -Wall -Wextra -I $(inc_dir)

#
# Debug build settings
#
DBGDIR = debug
DBGEXE = $(bin_dir)/$(DBGDIR)/$(EXE)
DBGOBJS = $(addprefix $(obj_dir)/$(DBGDIR)/, $(OBJS))
DBGCFLAGS = -g -O0 -DDEBUG

#
# Release build settings
#
RELDIR = release
RELEXE = $(bin_dir)/$(RELDIR)/$(EXE)
RELOBJS = $(addprefix $(obj_dir)/$(RELDIR)/, $(OBJS))
RELCFLAGS = -O3 -DNDEBUG

.PHONY: all clean debug prep release remake test

# Default build
all: prep release

#
# Debug rules
#
debug: $(DBGEXE)

$(DBGEXE): $(DBGOBJS)
	$(CC) $(CFLAGS) $(DBGCFLAGS) -o $(DBGEXE) $^

$(obj_dir)/$(DBGDIR)/%.o: %.c $(includes)
	$(CC) -c $(CFLAGS) $(DBGCFLAGS) -o $@ $<

#
# Release rules
#
release: $(RELEXE)

$(RELEXE): $(RELOBJS)
	$(CC) $(CFLAGS) $(RELCFLAGS) -o $(RELEXE) $^

$(obj_dir)/$(RELDIR)/%.o: %.c $(includes)
	$(CC) -c $(CFLAGS) $(RELCFLAGS) -o $@ $<

#
# Other rules
#
prep:
	@mkdir -p $(obj_dir)/$(DBGDIR) $(obj_dir)/$(RELDIR) $(bin_dir)/$(DBGDIR) \
	 $(bin_dir)/$(RELDIR) $(obj_dir)/$(DBGDIR)/processors $(obj_dir)/$(RELDIR)/processors

remake: clean all

clean:
	rm -f $(RELEXE) $(RELOBJS) $(DBGEXE) $(DBGOBJS)

test:
	@echo $(mkfile_path)
	@echo $(obj_dir)
	@echo $(bin_dir)
	@echo $(src_dir)
	@echo $(inc_dir)
	@echo $(includes)