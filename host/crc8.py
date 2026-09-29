def crc8(data: bytes) -> int:

  c8 = 0x07
  crc = 0x00

  for i in data:
    crc ^= i
    for _ in range(8):
      if (crc & 0x80) == 0:
        crc = crc << 1
      else:
        crc = crc << 1
        crc = (crc & 0xFF) ^ c8

  return crc


if __name__ == "__main__":
  print(hex(crc8(b"123456789")))