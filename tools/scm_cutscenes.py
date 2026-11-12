"""Read the opening retail SCM using the checked-in reVC command definitions."""
import re, struct, subprocess
from pathlib import Path
from functools import lru_cache
from stage_scripts import inspect
ROOT=Path(__file__).resolve().parents[1]

@lru_cache(maxsize=1)
def command_metadata():
    source=ROOT/'external/reVC/src/control'
    enum=subprocess.check_output(['cpp','-P',str(source/'ScriptCommands.h')],text=True)
    names=re.findall(r'COMMAND_\w+',enum);meta={}
    for m in re.finditer(r'REGISTER_COMMAND\((COMMAND_\w+), INPUT_ARGUMENTS\((.*?)\), OUTPUT_ARGUMENTS\((.*?)\)',(source/'ScriptDebug.cpp').read_text()):
        if m[1] in names:meta[names.index(m[1])]=(m[1][8:],re.findall(r'ARGTYPE_\w+',m[2]+m[3]))
    # ScriptDebug's signature omits the radius and move-state arguments.
    # Script.cpp collects six parameters for CHAR_FOLLOW_PATH.
    for opcode,(name,args) in list(meta.items()):
        if name=='CHAR_FOLLOW_PATH':meta[opcode]=(name,args+['ARGTYPE_FLOAT','ARGTYPE_INT'])
    return meta

def commands(data,index,start_offset=0):
    meta=command_metadata()
    info=inspect(data);offsets=info['offsets']+[len(data)];body=data[:info['main_bytes']] if index is None else data[offsets[index]:offsets[index+1]];p=start_offset
    while p<len(body):
        start=p;raw,=struct.unpack_from('<H',body,p);p+=2
        if raw&32767 not in meta:raise ValueError(f'Unknown SCM opcode at {p-2}')
        name,args=meta[raw&32767];values=[]
        for arg in args:
            if arg=='ARGTYPE_STRING':values.append(body[p:p+8].split(b'\0')[0].decode());p+=8;continue
            t=body[p];p+=1
            fmt={1:'i',2:'H',3:'H',4:'b',5:'h',6:'f'}[t];value=struct.unpack_from('<'+fmt,body,p)[0];p+=struct.calcsize(fmt)
            values.append(('global',value) if t==2 else ('local',value) if t==3 else value)
        if name=='START_NEW_SCRIPT':
            # Script.cpp reads a zero-terminated argument list after the label.
            while body[p]!=0:
                t=body[p];p+=1;fmt={1:'i',2:'H',3:'H',4:'b',5:'h',6:'f'}[t]
                value=struct.unpack_from('<'+fmt,body,p)[0];p+=struct.calcsize(fmt)
                values.append(('global',value) if t==2 else ('local',value) if t==3 else value)
            p+=1
        yield start,name,values

def scenes(data,index):
    models={93:'bka',94:'bkb',105:"wfyg1",106:"wfyg2"};objects={};scene=None;time=0;out=[]
    iterator=iter(commands(data,index))
    while True:
        try:at,name,args=next(iterator)
        except StopIteration:break
        except (ValueError,KeyError):
            if scene is not None:raise
            break
        if name=='LOAD_SPECIAL_CHARACTER':models[108+args[0]]=args[1].lower()
        elif name=='LOAD_SPECIAL_MODEL':models[args[0]]=args[1].lower()
        elif name=='LOAD_CUTSCENE':
            scene={'name':args[0].lower(),'objects':[],'subtitles':[],'script_address':at};out.append(scene);objects={};time=0
        elif name=='CLEAR_CUTSCENE':
            scene=None
            if index==2:break
        elif scene:
            if name=='SET_CUTSCENE_OFFSET':scene['origin']=args
            elif name=='CREATE_CUTSCENE_OBJECT':objects[args[1]]={'model':models[args[0]],'handle':args[1]};scene['objects'].append(objects[args[1]])
            elif name=='SET_CUTSCENE_ANIM':objects[args[0]]['animation']=args[1].lower()
            elif name=='ATTACH_CUTSCENE_OBJECT_TO_COMPONENT':objects[args[0]]['attach']=(args[1],args[2].lower())
            elif name=='ATTACH_CUTSCENE_OBJECT_TO_BONE':objects[args[0]]['bone']=(args[1],args[2])
            elif name=='IS_NUMBER_GREATER_THAN_INT_VAR' and args[1]==('global',496):time=args[0]
            elif name=='PRINT_NOW':scene['subtitles'].append({'time':time,'key':args[0],'duration':args[1]})
    return out
