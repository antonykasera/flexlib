# Arm cross-builds (M3; M4 and M7 kept building), the asm audit, sizes

ARM_CFLAGS = $(STD) $(WARN) $(WERROR) -O2 -g -ffunction-sections -fdata-sections \
             $(LIB_INC)
CROSS_CPUS = m3 m4 m7
CROSS_SRCS = $(LIB_SRCS) $(wildcard test/asm/*.c)
CROSS_OBJS = $(foreach c,$(CROSS_CPUS),$(patsubst %.c,$(BUILD)/cross/$(c)/%.o,$(CROSS_SRCS)))
M3_OBJS    = $(filter $(BUILD)/cross/m3/%,$(CROSS_OBJS))
M4_OBJS    = $(filter $(BUILD)/cross/m4/%,$(CROSS_OBJS))

define CROSS_RULE
$(BUILD)/cross/$(1)/%.o: %.c
	@mkdir -p $$(dir $$@)
	$(ARM_CC) $(2) $(ARM_CFLAGS) -MMD -MP -c $$< -o $$@
endef
$(eval $(call CROSS_RULE,m3,$(M3_FLAGS)))
$(eval $(call CROSS_RULE,m4,$(M4_FLAGS)))
$(eval $(call CROSS_RULE,m7,$(M7_FLAGS)))

cross: $(CROSS_OBJS)

# disassembly checks: M3 objects (audit.txt), M4 DSP-extension paths (audit_m4.txt)
asm-audit: $(M3_OBJS) $(M4_OBJS)
	$(PYTHON) tools/asm_audit.py --objdump $(ARM_OBJDUMP) --spec test/asm/audit.txt \
		$(M3_OBJS)
	$(PYTHON) tools/asm_audit.py --objdump $(ARM_OBJDUMP) --spec test/asm/audit_m4.txt \
		$(M4_OBJS)

# flash/RAM per library object (M3, -O2)
size: $(M3_OBJS)
	@objs="$(filter-out $(BUILD)/cross/m3/test/%,$^)"; \
	if [ -n "$$objs" ]; then $(ARM_SIZE) -t $$objs; else echo "no library objects yet"; fi

-include $(CROSS_OBJS:.o=.d)

.PHONY: cross asm-audit size
