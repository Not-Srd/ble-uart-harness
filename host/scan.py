import asyncio
from bleak import BleakScanner

async def main():
    found = await BleakScanner.discover(timeout=10.0, return_adv=True)
    print(len(found), "devices")
    for addr, (d, adv) in found.items():
        print(addr, adv.local_name, adv.rssi)

asyncio.run(main())