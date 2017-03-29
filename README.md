# NetOCD
Wireless JTAG and SWD firmware for ESP8266 and OpenOCD

(c) 2017 Lucas V. Hartmann <lhartmann@github.com>

Requires [xtensa/esp8266 toolchain](https://app.cear.ufpb.br/~lucas.hartmann/2016/08/24/ready-to-use-esp8266-toolchain-for-linux/) which you could build from souce, or just find a prebuilt one. My compiler is located as below, you will need to change `Makefile` if you choose to put your toolchain somewhere else.
```
/opt/espressif/xtensa-lx106-elf/bin/xtensa-lx106-elf-gcc
```

Requires [esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos). Use other_tools/cppchk.sh to fix required headers:
```bash
git clone https://github.com/SuperHouse/esp-open-rtos
cd esp-open-rtos
/path/to/cppchk.sh --fix
```

Create a symlink to the SDK on the root of the project, there must be a file `SDK/common.mk` for the project to build.
```bash
cd /path/to/NetOCD
ln -sf /path/to/esp-open-rtos SDK
make test
```

For use with a modified OpenOCD. Get and compile:
```
git clone https://github.com/lhartmann/openocd
./bootstrap
./configure --enable-netocd
make
```

Tested with a STM32F030F4P6 custom PCB, using SWD, no SRST or TRST connected. You may need to edit `openocd.cfg` for the right IP address. With some luck you will get your device capabilities.
```bash
/path/to/openocd -f openocd.cfg
```

