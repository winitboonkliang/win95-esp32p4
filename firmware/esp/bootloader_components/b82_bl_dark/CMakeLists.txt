idf_component_register(SRCS "bl_dark.c" REQUIRES hal esp_rom soc)
# force the weak-hook library into the bootloader link (see IDF examples/custom_bootloader/bootloader_hooks)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u bootloader_hooks_include")
