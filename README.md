# pidkiln
Ceramic kiln PID controller based on Arduino/Weemos

This is work in progress - but advanced enough to start uploading it.

## Required components:
- ESP32 board
- MAX31855 breakout board
- k-type termocouple
- 128x65 dot matrix LCD 12864B v2
- rotary encoder with button
- DC->AC solid state relay
- perhaps a kiln :)

## Key features:
- interface accessible both from LCD and WWW
- internal ESP storage for programs, data, logs
- online monitoring
- simply cool and cheap (comparing to commercially available products)

## Why this configuration?

I've already used "kind of" controllers made on Linux server and cron procedures, and it was ok for a while. Then I bought almost cheap Chinese controller, just to find out it is missing most of required stuff, and I already was accustom to be able to see everything over Internet.
So I've made few attempts with Arduino - it was fine, but since I need remote access - ESP will be much better choice. I've started to work on ESP8266, but being afraid that I will be lacking some GPIOs - I've moved to ESP32. And since price difference is negligible this is the platform of choice for this project.
It is also beneficial it has build in flash memory, that I can use for all required data - without need of connecting additional SD cards.

