##
## @file
## @brief Cron Makefile.
## @author James Humphrey (humphreyj@somnisoft.com)
##
## This software has been placed into the public domain using CC0.
##
.PHONY: all clean
.SUFFIXES:

BDIR      = build
CC        = cc
CFLAGS    = -O3 -Wall -Wextra -pedantic
COMPILE.c = $(CC) $(CFLAGS) -c -o $@ $<
LINK.c    = $(CC) $(CFLAGS) -o $@ $^

all: $(BDIR)/crond $(BDIR)/crontab

clean:
	rm -rf $(BDIR)

$(BDIR)/crond: $(BDIR)/crond.o $(BDIR)/cron.o
	$(LINK.c)
$(BDIR)/crond.o: src/crond.c | $(BDIR)
	$(COMPILE.c)
$(BDIR)/crontab: $(BDIR)/crontab.o $(BDIR)/cron.o
	$(LINK.c)
$(BDIR)/crontab.o: src/crontab.c | $(BDIR)
	$(COMPILE.c)
$(BDIR)/cron.o: src/cron.c | $(BDIR)
	$(COMPILE.c)
$(BDIR):
	mkdir -p $@

