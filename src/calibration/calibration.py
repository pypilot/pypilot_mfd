#!/usr/bin/env python
#
#   Copyright (C) 2024 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.  

import sys, os, time
import fcntl, serial
# these are not defined in python module
TIOCEXCL = 0x540C

# nmea uses a simple xor checksum
def nmea_cksum(msg):
    value = 0
    for c in msg: # skip over the $ at the begining of the sentence
        value ^= ord(c)
    return value & 255

def check_nmea_cksum(line):
    cksplit = line.split('*')
    try:
        return nmea_cksum(cksplit[0][1:]) == int(cksplit[1], 16)
    except:
        return False

gps_timeoffset = 0
def parse_nmea_gps(line):
    def degrees_minutes_to_decimal(n):
        n/=100
        degrees = int(n)
        minutes = n - degrees
        return degrees + minutes*10/6

    if line[3:6] != 'RMC':
        return False

    try:
        data = line[7:len(line)-3].split(',')
        if data[1] == 'V':
            return False
        gps = {}

        speed = float(data[6]) if data[6] else 0

        gps = {'speed': speed}
        if data[7]:
            gps['track'] = float(data[7])

    except Exception as e:
        print(_('nmea failed to parse gps'), line, e)
        return False

    return gps


'''
   ** MWV - Wind Speed and Angle
   **
   **
   **
   ** $--MWV,x.x,a,x.x,a*hh<CR><LF>**
   ** Field Number:
   **  1) Wind Angle, 0 to 360 degrees
   **  2) Reference, R = Relative, T = True
   **  3) Wind Speed
   **  4) Wind Speed Units, K/M/N
   **  5) Status, A = Data Valid
   **  6) Checksum
'''
def parse_nmea_wind(line):
    if line[3:6] != 'MWV':
        return False

    data = line.split(',')
    msg = {}
    try:
        msg['direction'] = float(data[1])
    except:
        return False  # require direction

    try:
        speed = float(data[3])
        speedunit = data[4]
        if speedunit == 'K': # km/h
            speed *= .53995
        elif speedunit == 'M': # m/s
            speed *= 1.94384
        msg['speed'] = speed
    except Exception as e:
        print(_('nmea failed to parse wind'), line, e)
        return False

    return msg

from pypilot.linebuffer import linebuffer
class NMEASerialDevice(object):
    def __init__(self, path):
        self.device = serial.Serial(*path)
        self.path = path
        self.device.timeout=0 #nonblocking
        fcntl.ioctl(self.device.fileno(), TIOCEXCL)
        self.b = linebuffer.LineBuffer(self.device.fileno())

    def readline(self):
        return self.b.readline_nmea()

    def close(self):
        self.device.close()

def compute():
    files = os.listdir()
    data = []
    for fn in files:
        if fn.startswith('wind_') and fn.endswith('.log'):
            print('using log file ', fn)
            f = open(fn)
            try:
                while True:
                    line = f.readline()
                    if not line:
                        break
                    line = line.rstrip()
                    data.append(list(map(float, line.split(', '))))
            except Exception as e:
                print('failed reading log file', e)
            f.close()
    print('data', data)
    x_data = list(map(lambda d : d[0], data))
    y_data = list(map(lambda d : d[1], data))

    import numpy as np

    
    # find curve fits
    #fits = map(lambda order : find_fit(order), [3])
    #find_fit(2)
    import matplotlib.pyplot as plt
    plt.plot(x_data, y_data, 'ro')

    def fit_order(order):
        p = np.polyfit(x_data, y_data, order)
        print('fit', order, p)
        x1 = np.linspace(0,10);
        y1 = np.polyval(p,x1)
        plt.plot(x1, y1)

        error = np.power(np.polyval(p, x_data) - y_data, 2)
        print('error', order, np.average(error))

    for o in [1, 3]:
        fit_order(o)
    
    #plt.plot([0, 10], [res[1], res[1]+res[0]*10])
    plt.show()
    
            
bauds = {'/dev/ttyACM0': 4800,
         '/dev/ttyUSB0': 115200}

def main():
    print('wind sensor calibration')
    print('without arguments record calibration data using available serial ports')
    print('with -c provide log filenames to generate plot data and caibration')

    if '-c' in sys.argv:
        print('computing calibration from logs')
        compute()
        return

    devices = {}
    for dev in os.listdir('/dev'):
        for serial_ports in ['ttyUSB', 'ttyAMA', 'ttyACM']:
            if dev.startswith(serial_ports):
                path = '/dev/' + dev;
                realpath = os.path.realpath(path)
                devices[realpath] = True

    if not devices:
        print('no serial devices found')
        return

    ports = []
    for device in devices:
        if device in bauds:
            print('using device', device, bauds[device])
            ports.append(NMEASerialDevice((device, bauds[device])))

    print('found %d ports' % len(ports))

    if len(ports) < 2:
        print('not enough data ports')
        exit(0)

    import datetime
    # Get the current date and time
    current_datetime = datetime.datetime.now()
    # Convert it to a string
    datetime_str = current_datetime.strftime("%Y-%m-%d_%H:%M:%S")
    logfile = 'wind_' + datetime_str + '.log'
    print('file', logfile)

    log = open(logfile, 'w')

    gps_speed_log = []
    wind_speed_log = []
    while True:
        for port in ports:
            while True:
                line = port.readline()
                if not line:
                    break

                result = parse_nmea_wind(line)
                if result and 'speed' in result:
                    print('wspd', result['speed'])
                    wind_speed_log.append(result['speed'])
                    continue
                
                result = parse_nmea_gps(line)
                if result and 'speed' in result:
                    speed = result['speed']
                    print('speed', speed)
                    if speed > 2:
                        for spd in gps_speed_log:
                            if abs(1- spd/speed) > .35:
                                print('gps clear', spd,speed,abs(1- spd/speed), len(gps_speed_log) )
                                gps_speed_log = []
                        gps_speed_log.append(speed)

                        if len(gps_speed_log) >= 4 and wind_speed_log:
                            # stable gps speed for 5 seconds with wind data
                            wind_speed = wind_speed_log[-1] # take last
                            line = str(speed) + ', ' + str(wind_speed)
                            print('log', line)
                            log.write(line + '\n')
                    wind_speed_log = [] # clear wind log with each gps
                    continue

                    
        time.sleep(0.1)

if __name__ == '__main__':
    main()
