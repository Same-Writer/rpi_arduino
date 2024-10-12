# rpi_arduino

Library intended to support Arduino remote command execution via simple serial commands from RaspberryPI

## HW Configuration

### Devices used:
- RaspberryPi 4

<img src="https://www.raspberrypi.com/documentation/computers/images/GPIO-Pinout-Diagram-2.png?hash=df7d7847c57a1ca6d5b2617695de6d46" width="800">


- Teensy 4.1 

<img src="https://www.pjrc.com/store/teensy41_card11a_rev3.png" width="500">


### HW Connections:
Power:
- Supply RPi power over USB-C
- RPi Pin4 (5V) --> Teensy Pin Vin (5V)
- RPi Pin6 (GND) --> Teensy Pin Gnd (GND)

Serial Connections:
- RPi Pin 8 (UART TXD) --> Teensy Pin 0 (UART RX1)
- RPi Pin 10 (UART RXD) --> Teensy Pin 1 (UART TX1)
- RPi Pin 14 (GND) --> Teensy Pin -1 / Gnd (GND)


## SW Setup

### Teensy 4.1 ([specs](https://www.pjrc.com/store/teensy41.html))
- Today, modifying teensy code and flashing the bitfile can't be done from the RPi. This is on the to-do list 
- Install the Arduino IDE + Teensyduino board support on a suitable host computer. [Instructions](https://www.pjrc.com/teensy/td_download.html)
- In the ArduinoIDE, install the [arduinojson](https://arduinojson.org/v7/how-to/install-arduinojson/#method-1-arduino-ide) library
- Connect your host computer to the Teensy via USB and make sure it is connected / discoverable
- Open the `bs_unit_impl.ino` sketck in the Arduino IDE, compile and upload.

### Raspberry Pi ([docs](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html))
- Any version of raspbian should work. Figure it out [here](https://www.raspberrypi.com/documentation/computers/getting-started.html)
- Connect the RPi to your WiFi for easy development. [SSH into the RPi](https://www.raspberrypi.com/documentation/computers/remote-access.html) for the subsequent instructions
- Using `raspi-config`, set the following:
	- enable ssh 
	- enable GPIO
	- enable serial (but disable login over serial)
    - reboot after you're done
- Install several updates and deps:
```
sudo apt-get update
curl -LsSf https://astral.sh/uv/install.sh | sh
```
- Give your user account access to the serial pins: 
```
sudo usermod -a -G dialout $USER
```
- Clone this repo:
```
cd ~
git clone git@github.com:Same-Writer/rpi_arduino.git
get checkout origin/main
cd ~/rpi_arduino
```
- Run the example code with uv:
```
uv run rpi_ard_comms.py
```


## Communication format

A "command" takes the form of a JSON string, which contains a header + optional arguments. This command is send over UART between rPI <--> Arduino

- The RaspberryPI must send a JSON string with at least two header fields, `function` and `cmdID`, to the Arduino
```
{"function": "do_nothing()", "cmdID": 00000}
```
- `cmdID` should be unique for each command in-flight. This will enable more advanced queueing mechanisms to handle asynchronous responses.
- The Arduino must have registered the function in question. This allows the Arduino to decode the function name and execute associated code. See details in Arduino source
- Upon receiving a command, the Arduino will send an ACK or NACK response, containing a `cmdID` and `status` to indicate whether the command maps to a correctly registered function:
```
# Correctly registered function + ACK (status=1)
        ARD01 <-- {"function": "do_nothing()", "cmdID": 00000}
        ARD01 --> {'cmdID': 00000, 'status': 1}

# Unregistered function + NACK (status=14)
        ARD01 <-- {"function": "unregd_fxn()", "cmdID": 00001}
        ARD01 --> {'cmdID': 00001, 't_exec_us': 14859, 'status': 14}
```
- Optional arguments can also be passed. These arguments are named by their datatype and are fixed in the Arduino implementation:

```
  // JSON input example:
  // {
  //  "function": "move_forward()",
  //  "cmdID": 12345,
  //  "intArg0": 420,
  //  "intArg0": 69,
  //  "intArg2": 0,
  //  "intArg3": -1,
  //  "floatArg0": 1.1,
  //  "floatArg1": 2.2,
  //  "strArg0": "abc",
  //  "strArg1": "def",
  //  }

```
All or none of these arguments may be present. Fields used are a best guess as what may be needed, and are expected to change as more functionality is added to the Arduino

