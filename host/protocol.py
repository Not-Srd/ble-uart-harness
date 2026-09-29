from crc8 import crc8
import serial, time

def build_frame(cmd, payload = b""):
  body = bytes([len(payload), cmd]) + payload
  return b"\xA5" + body + bytes([crc8(body)])


if __name__ == "__main__":

  ser = serial.Serial("COM9", 115200, timeout = 0.5)

  time.sleep(2) #witing for the ESP32 to reset and start the uart rx task since opening the same port hte esp is connected to resets it.
  ser.reset_input_buffer() #throw away the boot log text

  list_of_tests = [build_frame(0x01),                     # Normal PING
                   bytes([0xA5, 0x00, 0x01, 0x00]),       # Wrong CRC
                   build_frame(0x09),                     # Unknown CMD
                   build_frame(0x01, b"\x42"),            # PING with Payload
                   bytes([0xA5, 0x40]),                   # BAD LEN
                   bytes([0xA5, 0x00])]                   # Timeout (truncated frame)

  for i in list_of_tests:
    
    ser.write(i)
    time.sleep(0.1)
    data = ser.read(ser.in_waiting or 1)

    print(data.hex(" "))  