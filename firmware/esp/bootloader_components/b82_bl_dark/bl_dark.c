/* Backlight dark from the first bootloader instruction (lead 21 Nov): after power-on, an esptool/card-tool chip reset or any reset that
 * skips our app, the LCD backlight enable (GPIO23) floats until lcd_hw_init and the panel shows white (webcam: one frame 87-131 at those
 * resets). Drive it low here; the app lights it only after the black fill. A pad hold from lcd_bl_off() survives an esp_restart anyway. */
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "esp_rom_gpio.h"

#define PIN_LCD_BL 23

void bootloader_hooks_include(void) {}

void bootloader_before_init(void)
{
	gpio_ll_set_level(&GPIO, PIN_LCD_BL, 0);   /* level first, then drive: no high glitch */
	esp_rom_gpio_pad_select_gpio(PIN_LCD_BL);
	gpio_ll_output_enable(&GPIO, PIN_LCD_BL);
}
