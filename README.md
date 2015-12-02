# Group 13's Project

## Team
* Brian Ensor
* Robert Ladd
* Dean Moser
* Satchel Spencer

## Parts
#### You will need an ESP8266 module and some other parts to run this code. Here's what we're using:
* [Sparkfun ESP8266 Thing](https://www.sparkfun.com/products/13231)
* [SparkFun microSD Transflash Breakout](https://www.sparkfun.com/products/544)
* [SparkFun FTDI Basic Breakout - 3.3V](https://www.sparkfun.com/products/9873)

Refer to the [Sparkfun ESP8266 Thing Hookup Guide](https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/) for assembly instructions.

## Running the code
Some of these instructions were lifted from https://github.com/esp8266/Arduino. Go check out the project that lets us program this tiny chip in a familiar language.

1. [Download](https://www.arduino.cc/en/main/software) and install the Arduino IDE. (version 1.6.5)
2. Start Arduino and open Preferences window.
3. Enter http://arduino.esp8266.com/staging/package_esp8266com_index.json into Additional Board Manager URLs field. **It is important that you use the staging version because we use features not yet included in the stable version.**
4. Open Boards Manager from Tools > Board menu and install the esp8266 platform. Don't forget to select your ESP8266 board from Tools > Board menu after installation.
5. [Download](https://github.com/satchelspencer/13/archive/master.zip) the latest version of our code.
6. Add the files core_esp8266_features.h and libb64 from https://github.com/esp8266/Arduino/tree/master/cores/esp8266 to the folder below. **The code will not run without these additions.**
   * Mac: ````~/Library/Arduino15/packages/esp8266/hardware/esp8266/2.0.0/cores/esp8266/````
   * GNU/Linux ?: ````~/.arduino15/packages/esp8266/hardware/esp8266/2.0.0/cores/esp8266/````
   * Windows ?: ````%APPDATA%\Arduino15\packages\esp8266\hardware\esp8266\2.0.0\cores\esp8266\````
7. Install ArduinoJson Library with Library Manager
    - In Arduino IDE (since version 1.6.2) goto Sketch Tab -> Include Library -> Manage Libraries 
    - Search for ArduinoJson and click on it 
    - Select version and click install
8. [Download](https://github.com/marvinroger/ESP8266TrueRandom/archive/master.zip) the ESP8266TrueRandom library.
    - In the Arduino IDE, go to Sketch > Include Library > Add .ZIP Library...
    - Select the ESP8266TrueRandom-master.zip
    - Click Choose to install the library
9. [Download](https://github.com/Links2004/arduinoWebSockets/archive/master.zip) the arduinoWebSockets library.
    - Follow same instructions as above to install
10. Connect your ESP8266 to your computer.
11. Open the server/server.ino file in the Arduino IDE. Set the communication port in the Tools > Port menu.
12. That's it! Upload the sketch to your device (Sketch > Upload).

**If you are not using an SD card, follow [this guide](http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html#uploading-files-to-file-system) to install ESP8266FS and store the data directory on your device's flash memory.**
