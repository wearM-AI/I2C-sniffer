import sys
import glob
import serial
import time
import re



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



if __name__ == '__main__':
    ports = serial_ports()
    print(ports)
    print("Selected port: " + ports[-1])
    ser = serial.Serial(ports[-1], baudrate=115200)

    buffer_string = ""
    while True:
        buffer_string = buffer_string + str(ser.read(ser.inWaiting()))
        print(buffer_string)
        parseddata, buffer_string = parseData(buffer_string)
        # print(parseddata)
        time.sleep(0.1)

