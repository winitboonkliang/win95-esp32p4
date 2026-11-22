idf_component_register(SRCS "esp_main.c"
"../../fpu.c"
"../../i386.c"
"../../i8042.c"
"../../i8259.c"
"../../ide.c"
"../../ini.c"
"../../pc.c"
"../../misc.c"
"../../pci.c"
"../../vga.c"
"../../gfxcard.c"
"b29_present.c"
"b45_board.c"
"gsl/gsl_touch.c"
"gsl/gsl_point_id.c"
"../../ptrdev.c"
"../../pcspk.c"
"../../snapshot.c"
"lz4_dec.S"
"b61_bench.c"
# wifi.c is added conditionally below (BOARD-dependent)
# NE2000/8257/SB16/OPL sources: not in the jc8012p4a1c image (BOARD_LEAN)
"lcd_jd9365_p4.c"
"jd9365/esp_lcd_jd9365.c"
"jd9365/ppa_rotator.cpp"
"i2s.c"
"rtc.c"
"b86clock.c"
"storage.c"
"usb_hid.c"
"usb_input.c"
"cardsvc.c"
"inject.c"
"r29b_scn.c"
"eto_pie.S" "gfx_pie.S"
"b40_glue.c"
"b82doom.c" "b82_pie.S" "b85.c" "b85pad.c" "b85hid.c"
INCLUDE_DIRS "jd9365" "gsl"
LDFRAGMENTS "linker.lf")

if(NOT DEFINED BOARD)
    set(BOARD "jc8012p4a1c")
endif()

message(STATUS "Building for BOARD=${BOARD}")

add_definitions(-include ${CMAKE_CURRENT_SOURCE_DIR}/board_${BOARD}.h)

# Card service (USB-Serial/JTAG file push/patch, see cardsvc.c). Default ON; cmake -DCARD_SERVICE=0 removes it.
if(NOT DEFINED CARD_SERVICE)
    set(CARD_SERVICE 1)
endif()
if(CARD_SERVICE)
    add_definitions(-DCARD_SERVICE)
endif()

# Board profiler (tools/bench/prof): cmake -DB50_PROF=1
if(B50_PROF)
    add_definitions(-DB50_PROF)
    target_sources(${COMPONENT_LIB} PRIVATE "prof.c")
endif()

add_definitions(-DB40_RUST_TIMER)
add_definitions(-DI386_ENABLE_FPU)
# measurement builds: idf.py / rbuild ... -DCYCLE_EXACT=1 (exact cpu->cycle mid-batch, see i386.c; per-ISR / per-record instruction metrics need it)
if(CYCLE_EXACT)
    add_definitions(-DCYCLE_EXACT=1)
endif()
add_definitions(-DUSE_FMOPL)

# ── ESP32-P4 performance work (Doom-port lessons applied) ──────────────
# ESP-IDF compiles everything with -fno-jump-tables -fno-tree-switch-conversion.
# For a bytecode/opcode interpreter that is the single most expensive default
# in the build: the 256-way opcode switch degrades into a chain of compares.
# Re-enable real jump tables for the emulator hot files.
set(TINY386_FAST "-Wno-all -O3 -fjump-tables -ftree-switch-conversion -fno-strict-aliasing")
set_source_files_properties("../../adlib.c" PROPERTIES COMPILE_FLAGS "-Wno-all")
set_source_files_properties("../../fmopl.c" PROPERTIES COMPILE_FLAGS "-Wno-all -fsingle-precision-constant")
set_source_files_properties("../../fpu.c" PROPERTIES COMPILE_FLAGS "${TINY386_FAST}")
set_source_files_properties("../../i386.c" PROPERTIES COMPILE_FLAGS "${TINY386_FAST}")
set_source_files_properties("../../i8042.c" PROPERTIES COMPILE_FLAGS "-Wno-all")
set_source_files_properties("../../vga.c" PROPERTIES COMPILE_FLAGS "${TINY386_FAST}")
set_source_files_properties("../../pc.c" PROPERTIES COMPILE_FLAGS "${TINY386_FAST}")
set_source_files_properties("../../ne2000.c" PROPERTIES COMPILE_FLAGS "-Wno-all")
set_source_files_properties("../../sb16.c" PROPERTIES COMPILE_FLAGS "-Wno-all")
set_source_files_properties("../../i8257.c" PROPERTIES COMPILE_FLAGS "-Wno-all")

if(NOT EXISTS "../../fmopl.inc")
    message(FATAL_ERROR "Please run `make prepare` first")
endif()

# JC8012P4A1C builds without WiFi (no esp_wifi on P4 without esp_hosted).
if(NOT BOARD STREQUAL "jc8012p4a1c")
    target_sources(${COMPONENT_LIB} PRIVATE "wifi.c")
endif()

if(NOT BOARD STREQUAL "jc8012p4a1c")
    target_sources(${COMPONENT_LIB} PRIVATE "../../adlib.c" "../../fmopl.c" "../../ne2000.c" "../../i8257.c" "../../sb16.c")
endif()

# P1 pointer trace (rule 33 fixture): embedded byte for byte (BINARY adds no terminator; TEXT appended a NUL and changed the sha), sha256 checked at configure time
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/p1_trace.txt" P1_SHA)
if(NOT P1_SHA STREQUAL "8b43eaf108cb6b556300f7ca8aca89c865c349dfa9f56a6d173239b63a0d7c6c")
    message(FATAL_ERROR "esp/main/p1_trace.txt is not the fixture lab:judge/fixtures/pointer_trace_P1.txt (sha ${P1_SHA})")
endif()
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/p1_trace.txt" BINARY)
# B40: Win95 timekeeping = Rust 8254 (rust/b40, built by rust/b40/build_fw.bat -> rust/libb40.a); replaces i8254.c
# B60: own guest firmware (default ON; cmake -DB60_FW=0 = SeaBIOS from the bios.bin/vgabios.bin partitions).
# Host side = Rust fwhost (rust/fwhost) linked together with b40 in rust/libw95rs.a (rust/w95rs/build_fw.bat); the 64 KB guest BIOS
# (fw/bios.bin, built from fw/bios.S) is embedded in the app and copied to F0000 at reset.
if(NOT DEFINED B60_FW)
    set(B60_FW 1)
endif()
# B84 sound: every image with an engine (not the Windows image of the Doom-app layout, DOOM_ENGINE=none)
if(NOT DOOM_ENGINE STREQUAL "none")
    target_sources(${COMPONENT_LIB} PRIVATE "b84snd.c" "b84mix.c")
endif()
# B83 Doom app (doom partition, lab:LEAD/B83/PARTITIONS.md): -DB83_DOOM_APP=1 -DDOOM_ENGINE=both; boots straight into Doom.
# The Windows factory image of that layout: -DDOOM_ENGINE=none.
if(B83_DOOM_APP)
    add_definitions(-DB83_DOOM_APP)
endif()
if(B60_FW)
    add_definitions(-DB60_FW)
    target_link_libraries(${COMPONENT_LIB} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../rust/libw95rs.a)
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/../../fw/bios.bin" BINARY)
else()
    target_link_libraries(${COMPONENT_LIB} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../rust/libb40.a)
endif()

# GFX95 linear presenter (Rust, rust/present_core -> lib/libpresent_core.a; rebuild: rbuild.py rust/present_core --cargo ...)
target_link_libraries(${COMPONENT_LIB} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/libpresent_core.a)
# B45 pointer engine (Rust, rust/b45ptr -> lib/libb45ptr.a; rebuild: ptr/tools/build_rustlibs.sh)
target_link_libraries(${COMPONENT_LIB} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/libb45ptr.a)
set_source_files_properties("gsl/gsl_point_id.c" PROPERTIES COMPILE_FLAGS "-w")
# APPS B76 native apps (lab:APPS/B76/CONTRACT.md): stub trap port 0xF1E0 in pc.c, b76.ovl in ide.c, host app = Rust lib/libb76pad.a
# default ON (19 Nov): the host app allocates its PSRAM at the first stub trap only; cmake -DB76=0 = off
if(NOT DEFINED B76)
    set(B76 1)
endif()
if(B76)
    add_definitions(-DB76)
    target_sources(${COMPONENT_LIB} PRIVATE "b76glue.c")
    target_link_libraries(${COMPONENT_LIB} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/libb76pad.a)
endif()
