// Guition JC8012P4A1C — ESP32-P4, 10.1" 800x1280 MIPI-DSI (JD9365),
// 32 MB PSRAM, GSL3680 touch, ES8311 + NS4150 audio, USB-OTG host.
//
// Board layer by the Doom-on-ESP32-P4 project (github.com/winitboonkliang):
// the display path reuses its proven JD9365 bring-up and presents the
// logical 1280x800 framebuffer through the P4's PPA (hardware 90° rotate
// into the portrait DPI panel, zero CPU per pixel).
// Works with ESP-IDF v5.4.x.
#define BUILD_ESP32

// 32 MB PSRAM on this board.  The pool is a bump allocator that must cover
// guest RAM + guest VGA RAM + the 2 MB logical framebuffer; everything else
// (two 2 MB DPI frame buffers, USB, FatFs, stacks) comes out of the regular
// PSRAM heap afterwards.  22 MB leaves ~10 MB for those and lets the guest
// have 16 MB — the difference between Windows 95 swapping to the SD card on
// every window it opens and not swapping at all.
#define PSRAM_ALLOC_LEN (22 * 1024 * 1024)
// The pool comes from heap_caps_calloc: already zero, do not clear it again.
#define BOARD_POOL_ZEROED

// cpu_exec1() measures 135 KB and does not fit the IRAM segment, with the L2
// cache at 512 KB or at 128 KB — shrinking the cache frees L2MEM but not the
// region the linker places code into, so the link fails either way (tried).
// The hot HELPERS (tlb/translate/segcheck/flags) are IRAM_ATTR and do fit,
// which is where most of the win is.
#define IRAM_ATTR_CPU_EXEC1

#define BPP 16
#define FULL_UPDATE
// Whole-frame present via PPA instead of strip lcd_draw calls.
#define BOARD_FULLFRAME_PRESENT
#define USE_LCD_JD9365_P4
#define LCD_WIDTH 1280
#define LCD_HEIGHT 800

// microSD (SDMMC 4-bit) — same Guition family wiring as the 4.8" board,
// proven on this exact board in the OSOT project (LDO4 powers the slot).
#define SD_CLK 43
#define SD_CMD 44
#define SD_D0 39
#define SD_D1 40
#define SD_D2 41
#define SD_D3 42
#define SD_PWR_CTRL_LDO_IO_ID 4

// ES8311 codec + NS4150 amp.  NOTE: on the 10.1" board the PA enable is
// GPIO20 (the 4.8" uses 11) — confirmed on hardware.
#define MIXER_BUF_LEN 512
#define I2S_MCLK 13
#define I2S_BCLK 12
#define I2S_WS   10
#define I2S_DOUT 9
#define I2S_NUM  0

#define USE_ES8311
#define ES8311_PA 20
#define ES8311_I2C_NUM 0
#define ES8311_I2C_SDA 7
#define ES8311_I2C_SCL 8

// v1: no hosted WiFi (ESP32-C6 SDIO) — keeps us on ESP-IDF 5.4.x and all
// PSRAM bandwidth for the emulator.  NE2000 can come later.
// Also excludes wifi.c and the esp_hosted dependency entirely: esp_hosted's
// auto-init grabs the SDMMC host for its SDIO link and breaks the SD card.
#define BOARD_NO_WIFI

// USB keyboard/mouse on by default (no enable_usb line needed in the ini).
#define BOARD_USB_DEFAULT_ON

// Board image carries no NE2000, 8257 DMA, SB16/OPL (+ i2s mixer task), UART, floppy
// (emulink) or ATAPI CD: Win95 here uses none of them (SKIP_MAP row 8).
// Remove this define to get them back (the code stays in the tree).
#define BOARD_LEAN
