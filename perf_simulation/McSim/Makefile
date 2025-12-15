TARGET = mcsim

.PHONY: mcsim clean clean_mcsim

mcsim: $(TARGET).mk obj_mcsim
	@$(MAKE) TAG=mcsim -C . -f $(TARGET).mk

all: mcsim

obj_mcsim:
	mkdir $@

clean: clean_mcsim

clean_mcsim: obj_mcsim
	@$(MAKE) TAG=mcsim -C . -f $(TARGET).mk clean
	rm -rf $<
