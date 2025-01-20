import sys
import glob
import serial
import time
import re
from tkinter import *
import tkinter as tk 
import threading
from threading import Thread, Lock


def serial_ports():
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result



def addToParsed(parsed, new):
    if len(new) == 0:
        # parsedMsg[-1].append('')
        pass
    else:
        parsed[-1].append(int(new, 2))
    return parsed

def parseData(buffer):
    parsedMsg = dict()
    buffer = buffer.replace('b', '')
    # buffer.replace('\\', '')
    # buffer.replace('n', '')
    buffer = buffer.replace('\'', '')
    # buffer = re.sub(r"[^sSWR01+-]", "", buffer)


    # Is there a message start?
    lidx =  buffer.find('\\n')
    line = ""
    if lidx != -1:
        line = buffer[0:lidx]
    else:
        return parsedMsg, buffer
    
    matches = re.findall('t(.*?)i(.*?)d0:(.*?)d1:(.*?)d2:(.*?)d3:(.*?)d4:(.*?)d5:(.*?)d6:(.*?)', line, re.DOTALL)

    if(len(matches) == 0):
        buffer = buffer[lidx+2:]
        return parsedMsg, buffer
    
    if(len(matches[0]) != 9):
        print("faulty msg")
        buffer = buffer[lidx+2:]
        return parsedMsg, buffer


    parsedMsg["type"] = matches[0][0]
    parsedMsg["idx"] = matches[0][1]
    parsedMsg["d0"] = matches[0][2]
    parsedMsg["d1"] = matches[0][3]
    parsedMsg["d2"] = matches[0][4]
    parsedMsg["d3"] = matches[0][5]
    parsedMsg["d4"] = matches[0][6]
    parsedMsg["d5"] = matches[0][7]
    parsedMsg["d6"] = matches[0][8]

    buffer = buffer[lidx+2:]
    return parsedMsg, buffer


REC_DATA_TYPE = 2


buffer_string = ""
mutex = Lock()
lastTry = 0
MIN_RETRY_PERIOD = 10

if __name__ == '__main__':
    # ports = serial_ports()
    # print(ports)
    # print("Selected port: " + ports[-1])
    # if ports[-1] != "COM4":
    #     ser = serial.Serial(ports[-1], baudrate=115200)

    ser = []
    # active = False

    def openPort_th():
        global ser, active
        # active = True
        ports = serial_ports()
        if ports[-1] != "COM4":
            print("Selected port: " + ports[-1])
            try:
                ser = serial.Serial(ports[-1], baudrate=115200)
            except:
                    pass
        # active = False
        # mutex.release()

    

    def openPort():
        # with mutex:
        global lastTry
        # if mutex.acquire(blocking=False):
        if (time.time() - lastTry) >= MIN_RETRY_PERIOD:
            lastTry = time.time()
            t1 = threading.Thread(target=openPort_th)
            t1.start()


    openPort()
    root = tk.Tk()
    root.title("Simple GUI")
    # Display numeric values

    # recNo = -1 # Example numeric value 1
    recTxt = StringVar()
    recTxt.set("Rec: -1")
    value2 = 20  # Example numeric value 2
    label1 = tk.Label(root, textvariable=recTxt)
    label1.pack()

    def task():
        # print("hello")
        global buffer_string, ser
        root.after(100, task)
        failed = FALSE
        try:
            buffer_string = buffer_string + str(ser.read(ser.inWaiting()))
        except Exception as e:
            print(e)
            failed = True
            openPort()
        

        if failed:
            recTxt.set("Rec: -1")
        print(buffer_string)
        parseddata, buffer_string = parseData(buffer_string)
        if len(parseddata) == 0:
            return
        
        if(parseddata["type"] == str(REC_DATA_TYPE)):
            recNo = parseddata["d0"]
            recTxt.set(f"Rec: {recNo}")
        

    # Create and pack the labels


    # label2 = tk.Label(root, text=f"Value 2: {value2}")
    # label2.pack()

    # Run the application
    root.after(100, task)  # reschedule event in 2 seconds
    root.mainloop()



