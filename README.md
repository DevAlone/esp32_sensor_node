# esp32_sensor_node

Code for sensor node collecting data from sensors and sending to server(s).

## How to use

- Install esp mdf - https://docs.espressif.com/projects/esp-mdf/en/latest/get-started/
- Clonse this repository
- Change settings using `make menuconfig`
- Do `make clean` to apply them 
- Flash your device `make -j 4 flash` where j is number of threads in your system
- (Optional) See logs to know what is happening `make monitor` (use `Ctrl + ]` to exit)
