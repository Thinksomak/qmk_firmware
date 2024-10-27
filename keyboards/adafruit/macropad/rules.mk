# Build Options
#   change yes to no to disable


VIA_ENABLE = yes
BOOTLOADER = rp2040
OPT_DEFS += -DNO_DEBUG
BOOTMAGIC_ENABLE = no      # Enable Bootmagic Lite
MOUSEKEY_ENABLE = no        # Mouse keys
EXTRAKEY_ENABLE = yes       # Audio control and System control
CONSOLE_ENABLE = no         # Console for debug
COMMAND_ENABLE = no         # Commands for debug and configuration
NKRO_ENABLE = no           # Enable N-Key Rollover
BACKLIGHT_ENABLE = no       # Enable keyboard backlight functionality
RGBLIGHT_ENABLE = no        # Enable keyboard RGB underglow
AUDIO_ENABLE = no           # Audio output
# AUDIO_DRIVER = pwm_hardware
ENCODER_ENABLE = yes
RGB_MATRIX_ENABLE = no
OLED_ENABLE = yes
OLED_TRANSPORT = spi
