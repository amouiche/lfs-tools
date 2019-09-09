## LittleFS image creation/extraction utility package.

- Extract of mklfs original tool from https://github.com/whitecatboard/Lua-RTOS-ESP32/tree/master/components/mklfs/src
- Upgrade of littlefs from https://github.com/ARMmbed/mbed-os.git at revision 9da5c2227af8
- Creation of dumplfs tool

Usage:

```
mklfs -c <pack-dir> -b <block-size> -r <read-size> -p <prog-size> -s <filesystem-size> -i <image-file-path>

dumplfs -b <block-size> -r <read-size> -p <prog-size> -i <image-file-path> -o <output-dir>
```

