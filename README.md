A driver for an Arduino radio control panel (TFT version) for FlightGear, the open source flight simulator.

The Arduino source code is in `radioPanelTFT.ino`. Check the headers for building instructions.

Note: `radioPanel7S.old` is an old version that worked with five 7-segment modules. Provided for reference.

# Installation

In Windows, install Python3 from the Microsoft store and then:

```
pip3 install -U serial
pip3 install -U telnet
```

# Running

Run Flightgear with `--telnet 9000` and then:

```
python3 radioModule.py --host localhost --port 9000 --serial COM3 --conf c172p.ini
```