#!/usr/bin/env python3
import argparse, hashlib, json, pathlib, shutil, subprocess, sys, datetime, base64, struct, binascii
p=argparse.ArgumentParser(); p.add_argument('--sha',default='local'); a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
site=root/'site'; shutil.rmtree(site,ignore_errors=True); shutil.copytree(root/'web',site)
out=site/'firmware'; out.mkdir()
packages=pathlib.Path.home()/'.platformio/packages'
build=root/'.pio/build/cores3'
merged=out/'cores3-merged.bin'
subprocess.run([sys.executable,str(packages/'tool-esptoolpy/esptool.py'),'--chip','esp32s3','merge_bin','-o',str(merged),'--flash_mode','dio','--flash_freq','80m','--flash_size','16MB','0x0',str(build/'bootloader.bin'),'0x8000',str(build/'partitions.bin'),'0xe000',str(packages/'framework-arduinoespressif32/tools/partitions/boot_app0.bin'),'0x10000',str(build/'firmware.bin')],check=True)
version='1.1.0+'+a.sha[:7]
manifest={'name':'CoreS3 Type2DK I2C Check','version':version,'new_install_prompt_erase':True,'new_install_improv_wait_time':0,'builds':[{'chipFamily':'ESP32-S3','parts':[{'path':'firmware/cores3-merged.bin','offset':0}]}]}
(site/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
digest=hashlib.sha256(merged.read_bytes()).hexdigest()
(site/'build-info.json').write_text(json.dumps({'version':version,'commit':a.sha,'sha256':digest,'bytes':merged.stat().st_size,'built_at':datetime.datetime.now(datetime.timezone.utc).isoformat()},indent=2)+'\n')
(out/'SHA256SUMS').write_text(digest+'  cores3-merged.bin\n')
assert merged.read_bytes()[0]==0xe9
assert 65536 < merged.stat().st_size < 16*1024*1024
print('Packaged',version,digest)

# Preserve a build-checked target image without redistributing the licensed SDK.
# Physical boot and I2C operation still require a board test.
slave_name='2dk_i2c_diag_v5.bin'
slave=base64.b64decode(''.join((root/'type2dk'/f'{slave_name}.b64').read_text().split()),validate=True)
expected=(root/'type2dk/SHA256SUMS.txt').read_text().split()[0]
assert hashlib.sha256(slave).hexdigest()==expected, '2DK hash mismatch'
assert sum(struct.unpack_from('<8I',slave)) & 0xffffffff == 0
assert struct.unpack_from('<I',slave,32)[0]==0x98447902
assert struct.unpack_from('<I',slave,40)[0]==binascii.crc32(slave[:40]) & 0xffffffff
image_info=struct.unpack_from('<I',slave,36)[0]
assert image_info+32==len(slave)
assert struct.unpack_from('<I',slave,image_info)[0]==0xBB0110BB
assert struct.unpack_from('<I',slave,image_info+12)[0]==len(slave)
(out/slave_name).write_bytes(slave)
with (out/'SHA256SUMS').open('a') as f: f.write(expected+'  '+slave_name+'\n')
print('Verified 2DK v5:',len(slave),'bytes',expected)
