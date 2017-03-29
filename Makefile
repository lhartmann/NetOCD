PATH:=/opt/espressif/xtensa-lx106-elf/bin/:$(PATH)
ESPBAUD=500000
PROGRAM=esplink_gpio
CXXFLAGS += -std=gnu++11
EXTRA_COMPONENTS += extras/rboot-ota

include SDK/common.mk
