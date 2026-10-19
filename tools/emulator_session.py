#!/usr/bin/env python3
"""Start isolated DeSmuME with workspace-local Xvfb and input tools."""
import os
import subprocess
import time
import argparse
import shutil
import json
import tomllib
import tempfile
import hashlib
from pathlib import Path

root=Path(__file__).resolve().parents[1]
work=root/'build/emulator'
env=os.environ.copy()
display=next(n for n in range(110,170) if not Path(f'/tmp/.X11-unix/X{n}').exists() and not Path(f'/tmp/.X{n}-lock').exists())
env.update(DISPLAY=f':{display}',LD_LIBRARY_PATH=str(work/'local/usr/lib'),
           XDG_CONFIG_HOME=str(work/'config'),ALSOFT_DRIVERS='null',GTAVC_EMU_DUMP=str(work/'guest-main.bin'))
xdotool=str(work/'local/usr/bin/xdotool')
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--storage',choices=['retail','r4','cflash'],default='retail')
parser.add_argument('--rom',type=Path)
parser.add_argument('--debug',action='store_true')
parser.add_argument('--jit',action='store_true')
parser.add_argument('--emulator',choices=['desmume','melon'],default='melon')
args=parser.parse_args()
if args.rom is None:
    args.rom=work/'fat/rom.nds';args.rom.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(root/'build/revc-nds/revc_dsi.nds',args.rom)
if args.emulator=='melon':
    # NitroFS locates /rom.nds on the emulated SD card. Boot the same file
    # that FolderSync exposes so executable and embedded asset offsets agree.
    mounted_rom=work/'fat/rom.nds';mounted_rom.parent.mkdir(parents=True,exist_ok=True)
    if args.rom.resolve()!=mounted_rom.resolve():shutil.copyfile(args.rom,mounted_rom)
    args.rom=mounted_rom
    config=work/'config/melonDS/melonDS.toml'
    if not config.exists():
        config.parent.mkdir(parents=True,exist_ok=True)
        keys={'A':88,'B':90,'X':83,'Y':65,'L':81,'R':87,'Up':16777235,'Down':16777237,
              'Left':16777234,'Right':16777236,'Start':16777220,'Select':16777248}
        config.write_text('[Emu]\nDirectBoot = true\nExternalBIOSEnable = false\nConsoleType = 0\n'
            '[DLDI]\nEnable = true\nReadOnly = true\nFolderSync = true\nImageSize = 0\n'
            'ImagePath = '+json.dumps(str(work/'melon-sd.bin'))+'\nFolderPath = '+json.dumps(str(work/'fat'))+'\n'
            '[Instance0.Keyboard]\n'+'\n'.join(k+' = '+str(v) for k,v in keys.items())+'\n')
# Folder synchronization does not enlarge an existing FAT image when the ROM grows.
if args.emulator=='melon':
    settings=tomllib.loads(config.read_text());storage=settings.get('DLDI',{})
    image=Path(storage.get('ImagePath',''))
    folder=Path(storage.get('FolderPath',str(work/'fat')))
    required=sum(p.stat().st_size for p in folder.rglob('*') if p.is_file())+16*1024*1024
    if not image.is_file() or image.stat().st_size<required:
        megabytes=1
        while megabytes*1024*1024<required:megabytes*=2
        # Virtual SD caches are disposable; keep new images off the project disk.
        key=hashlib.sha256(str(root).encode()).hexdigest()[:12]
        cache=Path(tempfile.gettempdir())/('revc-emulator-'+key)
        cache.mkdir(exist_ok=True)
        enlarged=cache/f'sd-{megabytes}m.bin'
        if not enlarged.exists() and shutil.disk_usage(cache).free<megabytes*1024*1024+64*1024*1024:
            raise RuntimeError('Not enough temporary space for the emulator virtual SD cache')
        config.write_text(config.read_text().replace(json.dumps(str(image)),json.dumps(str(enlarged))))
        print('Preparing temporary virtual SD cache for this ROM',flush=True)
log=open(work/'xvfb.log','w')
xvfb=subprocess.Popen([str(work/'local/usr/bin/Xvfb'),env['DISPLAY'],'-screen','0','800x1000x24','-nolisten','tcp'],env=env,stdout=log,stderr=log)
try:
    for _ in range(30):
        if xvfb.poll() is not None:raise RuntimeError('Xvfb exited; inspect xvfb.log')
        test=subprocess.run([xdotool,'getdisplaygeometry'],env=env,capture_output=True)
        if test.returncode==0:break
        time.sleep(.1)
    else:raise RuntimeError('Virtual display unavailable; inspect xvfb.log')
    logemu=open(work/'desmume.log','w')
    binary=work/'installed/bin/desmume-cli'
    command=[str(binary) if binary.exists() else 'desmume-cli','--disable-sound','--nojoy',
             '--num-cores','2','--frameskip','0','--3d-render','SW','--scale','2']
    if args.jit:command+=['--jit-enable','--jit-size','1']
    if args.storage=='r4':command+=['--slot1','R4','--slot1-fat-dir',str(args.rom.resolve().parent)]
    if args.storage=='cflash':command+=['--cflash-path',str(args.rom.resolve().parent)]
    command+=[str(args.rom.resolve())]
    if args.emulator=='melon':
        env['QT_QPA_PLATFORM']='xcb'
        env['QT_QPA_PLATFORM_PLUGIN_PATH']=str(work/'melonds/squashfs-root/usr/plugins')
        command=[str(work/'melonds/squashfs-root/AppRun'),str(args.rom.resolve())]
    if args.debug:command=['gdb','-batch','-ex','run','-ex','bt','--args',*command]
    else:command=['stdbuf','-oL','-eL',*command]
    emulator=subprocess.Popen(command,env=env,cwd=work,stdout=logemu,stderr=logemu,start_new_session=True)
    (work/'session.json').write_text(__import__('json').dumps({'display':env['DISPLAY'],'xvfb':xvfb.pid,'emulator':emulator.pid,'command':command},indent=2)+'\n')
    print('Emulator running on isolated display '+env['DISPLAY'],flush=True)
    print('Emulator exit:',emulator.wait(),flush=True)
finally:
    xvfb.terminate();xvfb.wait()
