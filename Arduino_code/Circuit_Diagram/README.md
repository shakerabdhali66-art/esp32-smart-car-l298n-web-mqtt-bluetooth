# Circuit Diagram

## Components
- ESP32 Dev Module
- L298N Motor Driver
- 4 DC Motors
- 12V Battery
- LEDs
- WiFi + Bluetooth + MQTT control

## Pin Connections

| L298N | ESP32 |
|------|------|
| ENA | GPIO21 |
| IN1 | GPIO4 |
| IN2 | GPIO23 |
| IN3 | GPIO18 |
| IN4 | GPIO19 |
| ENB | GPIO22 |

## Power

- Battery +12V → L298N 12V
- Battery GND → L298N GND
- L298N 5V → ESP32 VIN
- L298N GND → ESP32 GND

## Motors

- Left motors → OUT1 & OUT2
- Right motors → OUT3 & OUT4
