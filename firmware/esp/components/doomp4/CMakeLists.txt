# B82 Doom engine (doomgeneric, GPL v2: LICENSE) for WIN95_P4. Host-side native app: no x86. B84: sound in the Doom boot (esp/main/b84snd.c).
# Source = the Doom_P4_HD tree (earlier private port) (640x400 Crispy-style HD tree). -DDOOM_DG=<dir> builds another tree (the 320x200 A/B build uses
# the Doom_P4 tree (earlier private port) /components/doomgeneric/dg; read only) - DOOM_RESX/DOOM_RESY must match it.
if(NOT DEFINED DOOM_DG)
    set(DOOM_DG ${CMAKE_CURRENT_LIST_DIR}/dg)
endif()
if(NOT DEFINED DOOM_RESX)
    set(DOOM_RESX 640)
    set(DOOM_RESY 400)
endif()
set(DG_SRCS
    dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c
    d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c
    g_game.c hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c
    i_scale.c i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c
    m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c
    m_random.c p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c
    p_lights.c p_map.c p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c
    p_setup.c p_sight.c p_spec.c p_switch.c p_telept.c p_tick.c p_user.c
    r_bsp.c r_data.c r_draw.c r_main.c r_plane.c r_segs.c r_sky.c
    r_things.c sha1.c sounds.c statdump.c st_lib.c st_stuff.c s_sound.c
    tables.c v_video.c wi_stuff.c w_checksum.c w_file.c w_main.c w_wad.c
    z_zone.c w_file_stdc.c i_input.c i_video.c doomgeneric.c
    i_oplmusic.c midifile.c mus2mid.c)   # B84: OPL music (MUS -> MIDI -> GENMIDI voices)
list(REMOVE_ITEM DG_SRCS i_system.c m_config.c info.c sounds.c)
list(TRANSFORM DG_SRCS PREPEND "${DOOM_DG}/")
list(APPEND DG_SRCS hooks/i_system.c hooks/m_config.c hooks/info.c hooks/sounds.c)
# B84: Nuked OPL3 + chocolate-doom OPL layer, ESP backend pumped by the B84 mixer (esp/main/b84snd.c). From Doom_P4_HD, data2psram applied
set(OPL_SRCS opl/opl.c opl/opl_queue.c opl/fmopl.c opl/opl_esp.c)   # B84: fmopl (OPL2) - Nuked opl3.c needs ~1.2 cores (board B t5)
list(APPEND DG_SRCS ${OPL_SRCS})
if(EXISTS ${DOOM_DG}/b82_datainit.c)   # another tree carries its own list (tools/b82_gen_init.py <dg> <dg>/b82_datainit.c)
    list(APPEND DG_SRCS ${DOOM_DG}/b82_datainit.c)
else()
    list(APPEND DG_SRCS hooks/b82_datainit.c)
endif()   # B82 hooks, identical upstream in both trees
# B83 engine choice, DOOM_ENGINE (lab:LEAD/B83/REPORT.md, PARTITIONS.md):
#   rs   = the Rust engine (rust/doomrs -> esp/main/lib/libdoomrs_p.a, symbols rs_*; bit-exact with this C tree)   [default]
#   c    = the C engine below
#   both = C + Rust side by side: the Doom app image (doom partition), the engine is picked per launch
#   none = no engine: the Windows factory image of the Doom-app layout (the shortcut restarts into the doom partition)
# DOOM_RS=0 (older A/B scripts) = c.
if(NOT DEFINED DOOM_ENGINE)
    if(DEFINED DOOM_RS AND NOT DOOM_RS)
        set(DOOM_ENGINE c)
    else()
        set(DOOM_ENGINE rs)
    endif()
endif()
set(DOOMRS_LIB ${CMAKE_CURRENT_LIST_DIR}/../../main/lib/libdoomrs_p.a)
set(ENG_DEFS CMAP256 FEATURE_SOUND DOOMGENERIC_RESX=${DOOM_RESX} DOOMGENERIC_RESY=${DOOM_RESY})
if(DOOM_ENGINE STREQUAL "none")
    idf_component_register(INCLUDE_DIRS "${DOOM_DG}" "opl")
    target_compile_definitions(${COMPONENT_LIB} INTERFACE ${ENG_DEFS} B83_ENG_NONE)
    return()
endif()
if(DOOM_ENGINE STREQUAL "rs")
    idf_component_register(SRCS ${OPL_SRCS} INCLUDE_DIRS "${DOOM_DG}" "opl" LDFRAGMENTS "linker.lf" "linker_rs.lf")   # B84 OPL: shared host library
    target_link_libraries(${COMPONENT_LIB} INTERFACE ${DOOMRS_LIB})
    idf_build_set_property(__LDGEN_LIBRARIES "${DOOMRS_LIB}" APPEND)   # ldgen maps a prebuilt archive only when it is listed
    target_compile_definitions(${COMPONENT_LIB} INTERFACE ${ENG_DEFS} B83_ENG_RS)
    return()
endif()
if(DOOM_ENGINE STREQUAL "both")
    idf_component_register(SRCS ${DG_SRCS} INCLUDE_DIRS "${DOOM_DG}" "opl" LDFRAGMENTS "linker.lf" "linker_rs.lf")
    target_link_libraries(${COMPONENT_LIB} INTERFACE ${DOOMRS_LIB})
    idf_build_set_property(__LDGEN_LIBRARIES "${DOOMRS_LIB}" APPEND)
    target_compile_definitions(${COMPONENT_LIB} INTERFACE B83_ENG_C B83_ENG_RS)
else()
idf_component_register(SRCS ${DG_SRCS} INCLUDE_DIRS "${DOOM_DG}" "opl" LDFRAGMENTS "linker.lf")
    target_compile_definitions(${COMPONENT_LIB} INTERFACE B83_ENG_C)
endif()
target_compile_definitions(${COMPONENT_LIB} PUBLIC ${ENG_DEFS})
# B82: exit/error/zone/config hooks live in esp/main/b82doom.c (b82_quit, b82_error, b82_zone); I_Error must never exit() (= reboot Win95)
target_compile_definitions(${COMPONENT_LIB} PRIVATE B82_HOOKS)
# FOR_DOOM: IDF builds with -fno-jump-tables; the engine's big switches (P_*, info state dispatch) want real tables
target_compile_options(${COMPONENT_LIB} PRIVATE -Os -msmall-data-limit=0 -fno-strict-aliasing -fjump-tables -ftree-switch-conversion -w)
# flash budget (factory 1792K, Win95 uses 1.41 MB): -Os for the cold engine, -O3 only where the frame time goes (renderer + fixed math)
set_source_files_properties(opl/fmopl.c PROPERTIES COMPILE_OPTIONS "-O2;-fsingle-precision-constant")   # B84: the OPL core runs per sample
foreach(f r_draw.c r_plane.c r_segs.c r_bsp.c r_things.c r_main.c m_fixed.c)
    set_source_files_properties(${DOOM_DG}/${f} PROPERTIES COMPILE_OPTIONS "-O3")
endforeach()
