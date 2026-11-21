# cmake-check: a CMake project builds against the release archive, on the
# host (and runs, with an option set by the parent) and for the Cortex-M3;
# FLEXLIB_REQUIRE stops an old copy

CMAKE_CHECK = $(BUILD)/cmake
CMAKE_WARN  = -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion \
              -Wdouble-promotion -Werror
CMAKE_LIB   = -DFLEXLIB_DIR=$(CURDIR)/$(CMAKE_CHECK)/flexlib

cmake-check: dist
	rm -rf $(CMAKE_CHECK) && mkdir -p $(CMAKE_CHECK)
	tar xzf $(DIST) -C $(CMAKE_CHECK)
	cmake -S test/cmake -B $(CMAKE_CHECK)/host $(CMAKE_LIB) \
		-DFLEXLIB_REQUIRE=$(FLEXLIB_VERSION) -DCMAKE_C_COMPILER=$(HOST_CC) \
		-DCONSUMER_DEFINES=FU_CRC_SMALL -DCMAKE_C_FLAGS="$(CMAKE_WARN) -O2" > /dev/null
	grep -q -- -DFU_CRC_SMALL \
		$(CMAKE_CHECK)/host/flexlib/CMakeFiles/flexlib_utils.dir/flags.make
	cmake --build $(CMAKE_CHECK)/host
	$(CMAKE_CHECK)/host/app
	cmake -S test/cmake -B $(CMAKE_CHECK)/m3 $(CMAKE_LIB) \
		-DCMAKE_SYSTEM_NAME=Generic -DCMAKE_C_COMPILER=$(ARM_CC) \
		-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
		-DCMAKE_C_FLAGS="$(M3_FLAGS) $(CMAKE_WARN) -O2 -ffunction-sections -fdata-sections" \
		> /dev/null
	cmake --build $(CMAKE_CHECK)/m3
	cmake -S test/cmake -B $(CMAKE_CHECK)/old $(CMAKE_LIB) \
		-DFLEXLIB_REQUIRE=99.0.0 2>&1 | tr -s ' \n' ' ' | \
		grep -q "older than FLEXLIB_REQUIRE 99.0.0"
	@echo "cmake-check: host and Cortex-M3 builds ok"

.PHONY: cmake-check
