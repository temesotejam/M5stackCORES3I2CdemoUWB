#!/usr/bin/env python3
import argparse, hashlib, json, pathlib, shutil, subprocess, sys, datetime
p=argparse.ArgumentParser(); p.add_argument('--sha',default='local'); a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
site=root/'site'; shutil.rmtree(site,ignore_errors=True); shutil.copytree(root/'web',site)
out=site/'firmware'; out.mkdir()
packages=pathlib.Path.home()/'.platformio/packages'
build=root/'.pio/build/cores3'
merged=out/'cores3-merged.bin'
subprocess.run([sys.executable,str(packages/'tool-esptoolpy/esptool.py'),'--chip','esp32s3','merge_bin','-o',str(merged),'--flash_mode','dio','--flash_freq','80m','--flash_size','16MB','0x0',str(build/'bootloader.bin'),'0x8000',str(build/'partitions.bin'),'0xe000',str(packages/'framework-arduinoespressif32/tools/partitions/boot_app0.bin'),'0x10000',str(build/'firmware.bin')],check=True)
version='1.0.0+'+a.sha[:7]
manifest={'name':'CoreS3 Type2DK I2C Check','version':version,'new_install_prompt_erase':True,'new_install_improv_wait_time':0,'builds':[{'chipFamily':'ESP32-S3','parts':[{'path':'firmware/cores3-merged.bin','offset':0}]}]}
(site/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
digest=hashlib.sha256(merged.read_bytes()).hexdigest()
(site/'build-info.json').write_text(json.dumps({'version':version,'commit':a.sha,'sha256':digest,'bytes':merged.stat().st_size,'built_at':datetime.datetime.now(datetime.timezone.utc).isoformat()},indent=2)+'\n')
(out/'SHA256SUMS').write_text(digest+'  cores3-merged.bin\n')
assert merged.read_bytes()[0]==0xe9
assert 65536 < merged.stat().st_size < 16*1024*1024
print('Packaged',version,digest)
