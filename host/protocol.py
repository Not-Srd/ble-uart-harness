from crc8 import crc8
import serial, time

def build_frame(cmd, payload = b""):
  body = bytes([len(payload), cmd]) + payload
  return b"\xA5" + body + bytes([crc8(body)])

ser = serial.Serial("COM9", 115200, timeout = 0.5)
time.sleep(2) #witing for the ESP32 to reset and start the uart rx task since opening the same port hte esp is connected to resets it.
ser.reset_input_buffer() #throw away the boot log text

ser.write(build_frame(0x02))
time.sleep(0.2)
data = ser.read(ser.in_waiting or 1)
print(data.hex(" "))