import serial
import logging
import telnetlib

TELNET_EOL = '\r\n'

class FGFSConnection:
    def __init__(self, server, port):
        try:
            self.conn = telnetlib.Telnet(server, port)
        except ConnectionRefusedError:
            logging.error('Cannot connect to FlightGear. Running on debug mode')
            self.conn = None
        logging.info('Connected to: %s:%d'%(server, port))
    
    def send(self, text):
        logging.debug('Sending: %s'%text)
        if not self.conn:
            return
        self.conn.write(text.encode())
        self.conn.write(TELNET_EOL.encode())

    def sendAndGet(self, text):
        if not self.conn:
            return ''
        logging.debug('Sending: %s'%text)
        self.conn.write(text.encode())
        self.conn.write(TELNET_EOL.encode())
        return self.conn.read_until(TELNET_EOL.encode())

class PanelinoController:
    def __init__(self, ser, fgfs, conffile):
        self.ser = ser
        self.fgfs = fgfs
        self.fields = {}
        self.readConfiguration(conffile)
        self.getInitialValues()

    def run(self):
        while(True):
            line = self.ser.readline().strip()
            if line:
                name, value = line.decode().split('=', maxsplit=1)
                self.processValue(name, value)
    
    def readConfiguration(self, conffile):
        with open(conffile, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                name, prop, factor = line.split()
                self.fields[name] = dict(prop=prop, factor=int(factor))

    def getInitialValues(self):
        values = []
        for field in self.fields:
            prop = self.fields[field]['prop']
            try:
                response = self.fgfs.sendAndGet(f'get {prop}').decode()
                # response is: "PROPNAME = 'VALUE' (TYPE)"
                value = int(float(response.split("'")[1]) * self.fields[field]['factor'])
            except Exception as e:
                value = 0
                logging.warning(f'Cannot get value from prop: {prop} error={e}')
            logging.info(f'Init {field}={value}')
            values.append(value)
        self.ser.write(' '.join(map(str, values)).encode())

    def processValue(self, name, value):
        if name in self.fields:
            factor = self.fields[name]['factor']
            prop = self.fields[name]['prop']
            newvalue = float(value) / factor
            logging.info(f"{name}: {prop}={newvalue}")
            self.fgfs.send(f'set {prop} {newvalue}')


if __name__ == '__main__':
    import argparse
    parser = parser = argparse.ArgumentParser(description='A driver for the Arduino radio panel')
    parser.add_argument('-c', '--config', default='c172p.ini', help='Protocol configuration file')
    parser.add_argument('-s', '--serial', default='COM3', help='Serial port')
    parser.add_argument('-b', '--bauds', default=9600, type=int, help='Bauds for the serial communication')
    parser.add_argument('-H', '--host', default='localhost', help='FlightGear host')
    parser.add_argument('-p', '--port', default=9000, type=int, help='FlightGear port. Run fgfs with --telnet=9000')
    parser.add_argument('-v', '--verbose', default=False, action='store_true', help='Be verbose')
    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)

    with serial.Serial(args.serial, args.bauds, timeout=5) as ser:
        panelino = PanelinoController(ser, FGFSConnection(args.host, args.port), args.config)
        panelino.run()
