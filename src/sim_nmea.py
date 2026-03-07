#!/usr/bin/env python
#
#   Copyright (C) 2025 Sean D'Epagnier
#
# This Program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.

import time, random
import socket

host = '192.168.4.1'
port = 3600

wind_angle = 0
wind_speed = 0
pressure = 1.0
heading = 0
heading_rate = 0
water_speed = 0
rudder = 0

def clamp(value, min_val, max_val):
    return max(min(value, max_val), min_val)

def resolv(angle):
    while angle < 0:
        angle += 360
    while angle >= 360:
        angle -= 360
    return angle

# nmea uses a simple xor checksum
def nmea_cksum(msg):
    value = 0
    for c in msg: # skip over the $ at the begining of the sentence
        value ^= ord(c)
    return value & 255

def send_nmea(s, line):
    line = 'SM' + line
    msg = '$' + line + ('*%02X' % nmea_cksum(line)) + '\r\n'
    s.send(msg.encode())

def send_wind(s):
    global wind_angle, wind_speed
    line = 'MWV,%.1f,R,%.1f,K,A' % (wind_angle, wind_speed)
    send_nmea(s, line)

    wind_angle += (random.random() - 0.5) * 5.0
    wind_angle = resolv(wind_angle)

    wind_speed += (random.random() - 0.5)/10
    wind_speed = clamp(wind_speed, 0.0, 25.0)

def send_temp_pressure(s):
    global pressure
    line = 'XDR,P,%.1f,B,Barometer' % (pressure)
    send_nmea(s, line)

    pressure += random.random()/10
    pressure = clamp(pressure, 0.980, 1.030)

def send_heading(s):
    global heading, heading_rate

    line = 'HDM,%.3f,M' % heading
    send_nmea(s, line)

    line = 'ROT,%.3f,A' % heading
    send_nmea(s, line)
    
    heading += heading_rate/60.0
    heading = resolv(heading)

    heading_rate += (random.random() - 0.5)
    heading_rate = clamp(heading_rate, -10.0, 10.0)

def send_water_speed(s):
    global water_speed
    line = 'VHW,%.3f,N' % water_speed
    send_nmea(s, line)
    
    water_speed += random.random()/10
    water_speed = clamp(water_speed, 0.0, 6.0)
    
def send_rudder(s):
    global rudder
    line = 'RSA,%.3f,A' % rudder
    send_nmea(s, line)
    
    rudder += random.random()/10
    rudder = clamp(rudder, -20.0, 20.0)
    
def main():
    while True:
        s = None
        while True:
            print('connecting...')

            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(5)
                s.connect((host, port))
                print('connected')
                break
            except Exception as e:
                print('connection error', e)
            time.sleep(1)

        try:
            while True:
                for i in range(10):
                    send_wind(s)
                    time.sleep(0.1)
                send_heading(s)
                send_heading(s)
                send_rudder(s)
                send_temp_pressure(s)
        except Exception as e:
            print('failed to send', e)
        s.close()
        time.sleep(3)

if __name__ == '__main__':
    main()
