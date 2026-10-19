#!/usr/bin/env python3
"""Capture and send controls only to the isolated emulator display."""
import argparse
import os
import subprocess
import time
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];work=root/'build/emulator'
session=json.loads((work/'session.json').read_text())
env=os.environ.copy();env.update(DISPLAY=session['display'],LD_LIBRARY_PATH=str(work/'local/usr/lib'))
tool=str(work/'local/usr/bin/xdotool')
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('action',choices=['capture','touch','key','hold','windows','quit','status','resize','cleanup'])
parser.add_argument('value',nargs='?',default='screen.png');parser.add_argument('--seconds',type=float,default=1)
args=parser.parse_args()
def run(*argv):return subprocess.run([tool,*argv],env=env,check=True,capture_output=True,text=True).stdout.strip()
if args.action=='status':
    import json
    session=json.loads((work/'session.json').read_text())
    subprocess.run(['ps','-p',str(session['emulator']),'-o','pid,etime,time,pcpu,stat,comm'],check=True)
    for window in run('search','--name','DeSmuME|melonDS').splitlines():print(window,run('getwindowname',window))
elif args.action=='resize':
    window=run('search','--name','melonDS').splitlines()[0]
    run('windowsize',window,'512','810')
elif args.action=='windows':
    print(run('search','--name','.*'))
elif args.action=='capture':
    path=work/'captures'/Path(args.value).name
    windows=run('search','--onlyvisible','--name','DeSmuME|melonDS').splitlines()
    subprocess.run(['import','-display',env['DISPLAY'],'-window',windows[0],str(path)],env=env,check=True)
    print(path)
elif args.action=='touch':
    # Pixel coordinates inside this isolated emulator window, never the desktop.
    x,y=(int(v) for v in args.value.split(','))
    if not 0<=x<2048 or not 0<=y<2048:raise ValueError('Touch outside emulator window bounds')
    window=run('search','--onlyvisible','--name','melonDS').splitlines()[-1]
    run('windowfocus',window)
    run('mousemove','--window',window,str(x),str(y))
    run('mousedown','1');time.sleep(.2);run('mouseup','1')
elif args.action in ('quit','cleanup'):
    import json,signal
    # AppImage launchers can spawn children; stop only windows on our display.
    found=subprocess.run([tool,'search','--name','DeSmuME|melonDS'],env=env,capture_output=True,text=True)
    pids=set()
    for window in found.stdout.splitlines():
        try:pids.add(int(run('getwindowpid',window)))
        except (ValueError,subprocess.CalledProcessError):pass
    session=json.loads((work/'session.json').read_text());pids.add(session['emulator'])
    for pid in pids:
        try:os.kill(pid,signal.SIGTERM)
        except ProcessLookupError:pass
    try:os.kill(session['xvfb'],signal.SIGTERM)
    except ProcessLookupError:pass
    if args.action=='cleanup':
        # Remove only virtual servers launched from this workspace's binary.
        expected=str(work/'local/usr/bin/Xvfb').encode()
        for number in [97,*range(110,170)]:
            lock=Path(f'/tmp/.X{number}-lock')
            try:
                pid=int(lock.read_text().strip())
                argv=Path(f'/proc/{pid}/cmdline').read_bytes().split(b'\0')
                if argv[0]==expected and f':{number}'.encode() in argv:os.kill(pid,signal.SIGTERM)
            except (FileNotFoundError,ProcessLookupError,ValueError):pass
    for _ in range(50):
        try:os.kill(session['xvfb'],0)
        except ProcessLookupError:break
        time.sleep(.1)
else:
    windows=run('search','--name','DeSmuME|melonDS').splitlines()
    if not windows:raise RuntimeError('Emulator window not found')
    run('windowfocus',windows[-1])
    run('mousemove','--window',windows[-1],'100','100')
    keys=args.value.split('+')
    run('keydown',*keys)
    time.sleep(min(30,max(.05,args.seconds if args.action=='hold' else .15)))
    run('keyup',*keys)
