# generated sources (committed)

tables:
	$(PYTHON) tools/gen/gen_crc_tables.py > utils/src/fu_crc_tab.c
	$(PYTHON) tools/gen/gen_math_tables.py > math/src/fm_tables.c

.PHONY: tables
