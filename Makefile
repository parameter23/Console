# -----------------------------------------------------------------------------
# Top-level Makefile. The engine (include/, src/) has no main() of its own -
# it's a reusable library for the GameAPI (include/game.h). Each buildable
# firmware lives under examples/<name>/ with its own Makefile that compiles
# the shared engine sources together with that example's own files. This
# Makefile just delegates to the examples.
# -----------------------------------------------------------------------------
EXAMPLES = examples/dig-demo examples/zauberschloss examples/grogs-revenge examples/scroller-demo examples/invaders examples/nebelkrone examples/gamebook-template examples/loderunner

.PHONY: all clean flash docs docs-clean

all:
	@for d in $(EXAMPLES); do $(MAKE) -C $$d; done

clean:
	@for d in $(EXAMPLES); do $(MAKE) -C $$d clean; done

# Flashes the first example by default; run `make -C examples/<name> flash`
# to flash a specific one.
flash:
	$(MAKE) -C examples/dig-demo flash

docs:
	doxygen Doxyfile

docs-clean:
	rm -rf docs/html docs/latex
