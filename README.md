This is a radio and control panel for FlightGear, the open source flight simulator, using an Arduino UNO and a TFT module.

![](./panel.jpg)

You can control:

- Radio frequencies: COM1, COM2, NAV1, NAV2 (and their standby freqs); ADF (only current); Transponder.
- Dials: Course in the HI; radials of the OBS1, OBS2; QNH for the barometer.

# Souce code

The Arduino source code is in `radioPanelTFT.ino`. Check the headers for building instructions and pin configuration.
`radioPanel7S.old` is an old version that worked with five 7-segment modules. Provided for reference.

Several themes are available, but you must build the project to select one or the other: white on blue, green on black, orante on black.

`radioModule.py` is a Python dirver. You must run this driver in your PC. Windows are Linux are supported. See below.

All the source code and resources is under the GPL.

# Installation

You'll need a driver writen in Python for communications with FlightGear.

In Windows, install Python3 from the Microsoft store. In Linux, Python is already installed.

Run these commands from bash/powershell:

```
pip3 install -U serial
pip3 install -U telnet
```

You need running these commands only once.

# Running

Run Flightgear with `--telnet 9000`. Once FlightGear is loaded, run:

```
python3 radioModule.py --host localhost --port 9000 --serial COM3 --conf c172p.ini
```

Check the serial port for your system. `COM3` and `COM4` are typical serial ports for Arduino in Windows.
`/dev/ttyUSB0` or `/dev/ttyUSB1` are typical in Linux. A configuration file for the C172P aircraft is provided,
but most FA aircraft will be compatible with this configuration file.

- Left encoder: change selected item (COM-STB, NAV-STB, OBS...). Push: swap frequencies (in the modes that allow swapping: COM and NAV)
- Right encoder: change selected value. Push: toggle "small/big step" mode.
