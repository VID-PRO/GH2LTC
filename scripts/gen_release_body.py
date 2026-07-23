import sys, os

items = []
for f in sorted(os.listdir(sys.argv[1])):
    env_dir = os.path.join(sys.argv[1], f)
    if not os.path.isdir(env_dir):
        continue
    for fn in os.listdir(env_dir):
        if fn.endswith('-full.bin'):
            items.append((fn, os.path.join(env_dir, fn)))

chip_map = {}
device_map = {}
for fn, fp in items:
    full = fn.replace('-full.bin', '')
    if 'HDMI' in full:
        chip_map[full] = 'esp32p4'
        device_map[full] = 'ESP32-P4'
    else:
        chip_map[full] = 'esp32c3'
        device_map[full] = 'ESP32-C3'

body = '''## Flash Instructions

Download the \`*-full.bin\` file for your variant and flash with esptool:

'''

for fn, fp in items:
    full = fn.replace('-full.bin', '')
    chip = chip_map.get(full, 'esp32c3')
    body += f'### {full}\n\n'
    body += f'```\nesptool.py --chip {chip} write_flash 0x0 {fn}\n```\n\n'

body += '> These merged binaries include bootloader, partition table, OTA data, and application firmware at the correct offsets.\n'

print(body)
