esptool.py --chip esp32 --port /dev/tty.usbserial-TGJNV01G erase_flash

esptool.py --chip esp32 --port /dev/tty.usbserial-TGJNV01G write_flash 0x1000 /Users/igormarques/Downloads/ESP32-WROOM-32-AT-V3.4.0.0/ESP32-WROOM-32-AT-V3.4.0.0/bootloader/bootloader.bin

esptool.py --chip esp32 --port /dev/tty.usbserial-TGJNV01G write_flash 0x8000 /Users/igormarques/Downloads/ESP32-WROOM-32-AT-V3.4.0.0/ESP32-WROOM-32-AT-V3.4.0.0/partition_table/partition-table.bin

esptool.py --chip esp32 --flash_size --port /dev/tty.usbserial-TGJNV01G write_flash 0x10000 /Users/igormarques/Downloads/ESP32-WROOM-32-AT-V3.4.0.0/ESP32-WROOM-32-AT-V3.4.0.0/factory/factory_WROOM-32.bin


screen /dev/tty.usbserial-TGJNV01G 115200

=========================


esptool.py --chip esp32 --port /dev/tty.usbserial-TGJNV01G erase_flash

esptool.py --chip esp32 --port /dev/tty.usbserial-TGJNV01G write_flash 0x1000 //Users/igormarques/Downloads/ESP32-WROVER-32-AT-V2.2.0.0/ESP32-WROVER-V2.2.0.0/bootloader/bootloader.bin

esptool.py --chip esp32 --port /dev/tty.usbserial-TGJNV01G write_flash 0x8000 /Users/igormarques/Downloads/ESP32-WROVER-32-AT-V2.2.0.0/ESP32-WROVER-V2.2.0.0/partition_table/partition-table.bin

esptool.py --chip esp32 --port /dev/tty.usbserial-TGJNV01G write_flash 0x10000 /Users/igormarques/Downloads/ESP32-WROVER-32-AT-V2.2.0.0/ESP32-WROVER-V2.2.0.0/factory/factory_WROVER-32.bin


screen /dev/tty.usbserial-TGJNV01G 115200


==================
esptool.py --chip esp32 --port /dev/tty.usbserial-TGJNV01G erase_flash
cd //Users/igormarques/Downloads/ESP32-WROOM-32-AT-V3.4.0.0/ESP32-WROOM-32-AT-V3.4.0.0/
esptool.py --chip esp32 --flash_size 4MB --port /dev/tty.usbserial-TGJNV01G write_flash 0x1000 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin 0x10000 /factory/factory_WROOM-32.bin
==================
esptool.py --chip esp32 --port /dev/tty.usbserial-TGJNV01G erase_flash
cd //Users/igormarques/Downloads/ESP32-WROVER-32-AT-V2.2.0.0/ESP32-WROVER-V2.2.0.0/
esptool.py --chip esp32 --flash_size 4MB  --port /dev/tty.usbserial-TGJNV01G write_flash 0x1000 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin 0x10000 /factory/factory_WROVER-32.bin


=========
esptool.py --chip esp32 --port "/dev/cu.usbserial-TGJNV01G" --baud 921600  --before default_reset --after hard_reset erase_flash