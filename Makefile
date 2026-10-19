# FNIRSI DSO-152 Master Makefile

SUBDIRS = libdso152 examples/01_hello_world examples/02_breakout_game examples/03_mini_oscilloscope

all:
	@for dir in $(SUBDIRS); do \
		echo "Building $$dir..."; \
		$(MAKE) -C $$dir || exit 1; \
	done
	@echo "========================================================"
	@echo "  All SDK components and examples built successfully!  "
	@echo "========================================================"

bootloader:
	@echo "Building dso152_bootloader..."
	@$(MAKE) -C dso152_bootloader

clean:
	@for dir in $(SUBDIRS) dso152_bootloader; do \
		if [ -f $$dir/Makefile ]; then \
			echo "Cleaning $$dir..."; \
			$(MAKE) -C $$dir clean; \
		fi \
	done

.PHONY: all bootloader clean $(SUBDIRS)
