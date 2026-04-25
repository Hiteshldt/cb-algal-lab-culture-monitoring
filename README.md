CARBELIM LAB MONTIORING SYSTEM WITH DEVICE ID BTTE1250007, IT MONITOR AQI AND AIR PARAMETERS (12 VALUES), LIQUID VALUES INCLUDING pH, TURB, TDS



CODE GEMINI CHAT: https://gemini.google.com/share/cd94eb0c0e47

STM32 CODE WITH WINSEN, PH, TURB, TDS, SENSOR, AND COMM TO ESP32



CODE CONNECTION: 

Here is the simple, straightforward text breakdown of all the hardware connections based on your code:

1\. Board-to-Board Connection (STM32 to ESP32)

Make sure both boards share a common Ground (GND), or the data will be scrambled.

STM32 PC\_6 (TX) → Connects to → ESP32 GPIO 16 (RX)

STM32 PC\_7 (RX) → Connects to → ESP32 GPIO 17 (TX)

STM32 GND → Connects to → ESP32 GND

2\. STM32 Connections (Sensor Hub)

Water Sensors (Analog):

Turbidity Sensor (Data/Out) → Connects to → STM32 A2 (PA\_2)

pH Sensor (Data/Out) → Connects to → STM32 A1 (PA\_1)

TDS Sensor (Data/Out) → Connects to → STM32 A0 (PA\_0)

Power: Connect VCC of these sensors to 3.3V or 5V (depending on your specific sensor module) and GND to STM32 GND.

Air Sensor (ZPHS01B - UART):

STM32 PA\_9 (TX) → Connects to → ZPHS01B RX

STM32 PA\_10 (RX) → Connects to → ZPHS01B TX

ZPHS01B VCC → Connects to → 5V source (Required for the internal fan)

ZPHS01B GND → Connects to → STM32 GND

3\. ESP32 Connections (Display Hub)

ILI9341 TFT Display (SPI):

Display CS → Connects to → ESP32 GPIO 5

Display RESET → Connects to → ESP32 GPIO 4

Display DC / RS → Connects to → ESP32 GPIO 2

Display SDI/MOSI → Connects to → ESP32 GPIO 23

Display SCK → Connects to → ESP32 GPIO 18

Display SDO/MISO → Connects to → ESP32 GPIO 19 (Optional, only if reading from SD card)

Display VCC \& LED → Connects to → ESP32 3.3V

Display GND → Connects to → ESP32 GND



