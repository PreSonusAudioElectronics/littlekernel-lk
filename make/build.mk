# use linker garbage collection, if requested
ifeq ($(WITH_LINKER_GC),1)
GLOBAL_COMPILEFLAGS += -ffunction-sections -fdata-sections
GLOBAL_LDFLAGS += --gc-sections
endif

ifneq (,$(EXTRA_BUILDRULES))
-include $(EXTRA_BUILDRULES)
endif

$(EXTRA_LINKER_SCRIPTS):

$(OUTBIN): $(OUTELF)
	@echo generating image: $@
	$(NOECHO)$(SIZE) $<
	$(NOECHO)$(OBJCOPY) -O binary $< $@

$(OUTELF).hex: $(OUTELF)
	@echo generating hex file: $@
	$(NOECHO)$(OBJCOPY) -O ihex $< $@

$(EXTRA_LINKER_DEPS):

$(OUTELF): $(ALLMODULE_OBJS) $(LINKER_SCRIPT) $(EXTRA_LINKER_SCRIPTS) $(EXTRA_LINKER_DEPS)
	@echo linking $@
	$(NOECHO)$(SIZE) -t --common $(sort $(ALLMODULE_OBJS)) $(EXTRA_OBJS)
	@echo "pwd: ${PWD}\n"
	$(LD) $(GLOBAL_LDFLAGS) -dT $(LINKER_SCRIPT) $(addprefix -T,$(EXTRA_LINKER_SCRIPTS)) \
		$(ALLMODULE_OBJS) $(EXTRA_OBJS) $(LIBGCC) -Map=$(OUTELF).map -o $@ \
		--whole-archive $(addprefix -l,$(EXTRA_LINK_LIBS))

$(OUTELF).sym: $(OUTELF)
	@echo generating symbols: $@
	$(NOECHO)$(OBJDUMP) -t $< | $(CPPFILT) > $@

$(OUTELF).sym.sorted: $(OUTELF)
	@echo generating sorted symbols: $@
	$(NOECHO)$(OBJDUMP) -t $< | $(CPPFILT) | sort > $@

$(OUTELF).lst: $(OUTELF)
	@echo generating listing: $@
	$(NOECHO)$(OBJDUMP) -Mreg-names-raw -d $< | $(CPPFILT) > $@

$(OUTELF).debug.lst: $(OUTELF)
	@echo generating listing: $@
	$(NOECHO)$(OBJDUMP) -Mreg-names-raw -S $< | $(CPPFILT) > $@

$(OUTELF).dump: $(OUTELF)
	@echo generating objdump: $@
	$(NOECHO)$(OBJDUMP) -x $< > $@

$(OUTELF).size: $(OUTELF)
	@echo generating size map: $@
	$(NOECHO)$(NM) -S --size-sort $< > $@

# print some information about the build
$(BUILDDIR)/srcfiles.txt: $(OUTELF)
	@echo generating $@
	$(NOECHO)echo $(sort $(ALLSRCS)) | tr ' ' '\n' > $@

$(BUILDDIR)/include_paths.txt: $(OUTELF)
	@echo generating $@
	$(NOECHO)echo $(subst -I,,$(sort $(GLOBAL_INCLUDES))) | tr ' ' '\n' > $@

#include arch/$(ARCH)/compile.mk

