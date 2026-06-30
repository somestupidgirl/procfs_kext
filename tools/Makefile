CC=     cc
CFLAGS= -O2 -Wall -Wextra -std=c99 -g
PROGS=  procfs_ksyms procfsd

all: $(PROGS)

procfs_ksyms: procfs_ksyms.c
	@echo "Staging kernel symbols (libklookup)..."
	$(CC) $(CFLAGS) -o $@ -lcompression $<
	$(OUT)/procfs_ksyms || echo "procfs: symbol staging failed; klookup features disabled"

procfsd: procfsd.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)
	rm -rf *.dSYM

.PHONY: all clean
