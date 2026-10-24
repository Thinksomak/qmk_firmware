

#define OLED_DISPLAY_128X64
#define OLED_IC OLED_IC_SH1106
#define RGB_MATRIX_LED_COUNT 12

VIA_ENABLE = yes

BOOTLOADER = rp2040
OPT_DEFS += -DNO_DEBUG

ENCODER_MAP_ENABLE = yes		# Enable encoder
TAP_DANCE_ENABLE = yes			# Enable Tap Dance

BOOTMAGIC_ENABLE = no  			# Enable Bootmagic Lite
MOUSEKEY_ENABLE = yes			# Mouse keys
EXTRAKEY_ENABLE = yes   		# Audio control and System control
CONSOLE_ENABLE = no 			# Console for debug
COMMAND_ENABLE = no 			# Commands for debug and configuration
# NKRO_ENABLE = yes   			# Enable N-Key Rollover - Deprecated
NKRO_DEFAULT_ON = yes           # Enable N-Key Rollover
BACKLIGHT_ENABLE = no   		# Enable keyboard backlight functionality
# RGBLIGHT_ENABLE = no			# Enable keyboard RGB underglow
AUDIO_ENABLE = no   			# Audio output
# AUDIO_DRIVER = pwm_hardware
ENCODER_ENABLE = yes			# Enable encoder
RGB_MATRIX_ENABLE = yes 		# Enable per-key RGB matrix lighting
OLED_ENABLE = yes				# Enable OLED display
OLED_TRANSPORT = spi			# Enable SPI transport



# Pomodoro timer, stats, EEPROM storage, No-Sleep, and the OLED / RGB front ends
SRC += pomodoro.c storage.c stats.c nosleep.c boot_confirm.c ui_oled.c ui_rgb.c
# EEPROM user datablock: pomodoro settings + stats, 32 bytes (see storage.c).
# Bump the version whenever the layout changes; QMK then wipes the old block.
OPT_DEFS += -DEECONFIG_USER_DATA_SIZE=32 -DEECONFIG_USER_DATA_VERSION=0x504D0002
