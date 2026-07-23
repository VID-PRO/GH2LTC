import sys, os, subprocess

envs = {
    'TC-WL-LTC':  {'chip': 'esp32c3', 'flash_mode': 'dio', 'flash_size': '4MB',  'boot_ofs': '0x0'},
    'TC-WL-CLAP': {'chip': 'esp32c3', 'flash_mode': 'dio', 'flash_size': '4MB',  'boot_ofs': '0x0'},
    'TC-WL-HDMI': {'chip': 'esp32p4', 'flash_mode': 'qio', 'flash_size': '16MB', 'boot_ofs': '0x2000'},
}

for env, cfg in envs.items():
    build_dir = f'.pio/build/{env}'
    out_dir = f'merged/{env}'
    os.makedirs(out_dir, exist_ok=True)
    merged = os.path.join(out_dir, f'{env}-full.bin')

    boot = os.path.join(build_dir, 'bootloader.bin')
    part = os.path.join(build_dir, 'partitions.bin')
    app  = os.path.join(build_dir, 'firmware.bin')
    otad = os.path.join(build_dir, 'boot_app0.bin')

    if not os.path.exists(app):
        print(f'[WARN] {env}: firmware.bin not found, skipping')
        continue

    cmd = ['esptool.py', '--chip', cfg['chip'], 'merge_bin',
           '-o', merged,
           '--flash_mode', cfg['flash_mode'],
           '--flash_size', cfg['flash_size'],
           '--flash_freq', '80m']

    if os.path.exists(boot):
        cmd += [cfg['boot_ofs'], boot]
    else:
        print(f'[WARN] {env}: bootloader.bin not found, skipping')

    if os.path.exists(part):
        cmd += ['0x8000', part]
    else:
        print(f'[WARN] {env}: partitions.bin not found, skipping')

    if os.path.exists(otad):
        cmd += ['0xe000', otad]
    else:
        print(f'[WARN] {env}: boot_app0.bin not found, skipping')

    cmd += ['0x10000', app]

    print(f'[{env}] merging into {merged}')
    subprocess.run(cmd, check=True)

    size = os.path.getsize(merged)
    print(f'[{env}] merged size: {size} bytes ({size/1024/1024:.1f} MB)')
