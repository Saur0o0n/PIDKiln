# pidkiln
Ceramic kiln PID controller based on Arduino/Weemos board.

This is work in progress - but advanced enough to start uploading it.

## Key features:
- interface accessible both from LCD screen and WWW webpage
- unlimited (only by storage) kiln program number, program limited to 10k size (but this is artificial limit - can be extended)
- internal ESP storage for programs, data, logs (perhaps later SD - but I'm not sure yet)
- online monitoring, program management, editing, graphs and kiln control
- safety features build in (temperature run out protection, probe failure, SSR failure, kiln insulation failure)
- simply cool and cheap (comparing to commercially available products) all in one solution

## Required components:
- ESP32 board
- MAX31855 breakout board
- k-type thermocouple (1$+ - depending on max temperature)
- 128x65 dot matrix LCD 12864B v2
- rotary encoder with button
- DC->AC solid state relay
Kind of optional:
- thermistors
- perhaps a kiln :)

Total expenses for this set should be around 30-40$
- ESP32 board - 14-15$ if exactly like mine, but other ESP32 you can bought for 6$+
- MAX31855 board - 2$
- k-type thermocouple - 1$-10$ - depending on max temperature
- LCD - 5$
- encoder - 1$
- SSR - 4$ + 4$ for radiator

## Why this configuration?

I've already used "kind of" controller made on Linux server and cron procedures, and it was ok.. for a while. Then I bought almost cheap Chinese controller PC410, just to find out, it is missing most of required stuff (that original PC410 should have), and I already was accustom to be able to see everything over Internet.
So I've made few attempts with Arduino - it was fine, but since I need remote access - ESP will be much better choice. I've started to work on ESP8266, but being afraid that I will be lacking some GPIOs - I've moved to ESP32. And since price difference is negligible, this is the platform of choice for this project.
It is also beneficial, because it has build in flash memory, that I can use for all required data - without need of connecting additional SD cards.

MAX31855 comparing to more available MAX6675, is better choice since it allow us to work up to 1350C and it has 3,3V logic.

LC12864B is not the best choice, but I simply had this one already and used if before for 3d printer. Perhaps later I'll change it. Problem with this LCD is that it has 5V logic. Sometimes it works on 3,3V, in my case it does not. But since it's mostly one way communication hooking it up to 5V for both logic and back light works and does not crash my board. Clean solution would be to use logic voltage translator (there is plenty of them for 1$).

Some notes about SSR - I'm not sure yet, but perhaps I'll implement two stage SSR (probably SSR and mechanical relay) - just to be able to turn off remotely kiln if one of them fails. Anyway, if you are going to use cheap Chinese knock offs, make sure it rated twice the output current you will use.

Thermistors are to measure outside kiln temperature. In case of insulation failure - we can shut it of. This are extremely cheap (used in 3d printing) thermistors.

## GPIO/PINs connection

**LCD**

ESP32	| LCD
--------|---------
+3.3V	| BLA (this can be also +5V if you wish)
GND	| BLK
4	| RST
GND	| PSB
+5V	| VCC (This should be - for ESP sake - 3.3V, but my LCD doesn't work with lower voltage. Try first with 3.3V)
GND	| GND
5	| RS
18	| E
23	| R/W

**Encoder**

ESP32	| Encoder
--------|---------
+3.3V	| 5V/VCC
GND	| GND
32	| Key
34	| S2
35	| S1

**MAX31855**

EPS32	to MAX31855

**SSR**

ESP32	to SSR
GND	-> GND

**Thermistors**

ESP32	to thermistor

## Installation

- Assembly hardware, as specified above.
- Clone git into the Arduino user programs directory (on Linux "/home/username/Arduino/").
- You have to already have installed ESP32 framework - if don't, do it now (https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md).
- Don't forget about ESP32FS plugin (drop it to "/home/username/Arduino/tools")
- Install required additional libraries: [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer),[AsyncTCP](https://github.com/me-no-dev/AsyncTCP) 
- UPDate (there is no other way to do it) libraries/ESPAsyncWebServer/src/WebResponseImpl.h variable TEMPLATE_PLACEHOLDER to '~'
- Open PIDKiln.ino and edit your WiFi credentials (if you want to use). Compile and upload.
- Upload sketch data (from data directory) to ESP32 SPIFFS with help of ESP32FS plugin (Menu->Tools->ESP32 Sketch Data Upload)

## Documentation

- Most of the documentation you can find on [PIDKiln github page](https://github.com/Saur0o0n/pidkiln) - so please use it
- Some less formal uPDates information and step by step instructions will be on [my webpage](https://adrian.siemieniak.net/portal/tag/PIDKiln/)


