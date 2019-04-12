#!/usr/bin/env python3
"""This is a very primitive and dumb front end to get the ball
rolling.
"""
# -*- coding: utf-8 -*-
import argparse
import pdb
import serial
import sys
import time
import tkinter


def main(serial_port='/dev/ttyACM0', baudrate=9600, callsign=None):
    if not callsign:
        sys.stderr.write('*** no callsign provided ***\n')
        sys.exit(1)
        
    modem = serial.Serial(port=serial_port, baudrate=baudrate)
    
    def writeln(s):
        modem.write(s.encode('utf-8'))
        modem.write(b'\r\n')
        modem.flush()
        time.sleep(0.1)

    # On startup, we'll need to pull the '!BOOT OK' off the
    # console. If the modem has already started, there won't
    # be anything to read.
    modem.read(modem.in_waiting)
    writeln('!ID '+callsign)

    response = modem.read(modem.in_waiting).decode('utf-8').strip()
    print(response)
    assert(response == '*ID SET')
    

    root = tkinter.Tk()    
    frame = tkinter.Frame(root)
    frame.pack()

    messagelist = tkinter.Text(frame, width=64)
    messagelist.pack()
    
    message = tkinter.Text(frame)
    message.pack()
    
    transmitter = tkinter.Button(frame)
    transmitter["text"] = "Transmit"

    transmitter.pack()

    def transmit(*args):
        m = message.get("1.0", "end-1c")
        if not m:
            return
        writeln('>'+m)
        time.sleep(0.1)
        response = modem.read(modem.in_waiting).decode('utf-8').strip()
        assert(response == '*TX SENT')
        
    transmitter.bind('<Button-1>', transmit)

    root.mainloop()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(prog='lora-modem', description='gui for a lora modem')
    parser.add_argument('-p', '--port', default='/dev/ttyACM0', help='serial port')
    parser.add_argument('-c', '--callsign', default='', help='callsign')
    ap_args = parser.parse_args()
    
    main(serial_port=ap_args.port, baudrate=9600, callsign=ap_args.callsign)
    
    










