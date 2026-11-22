#!/bin/bash
# the three mode drivers: 1280x800 = B29GFX.DRV (b29gfx.drv), 1024x640 = B29G1024.DRV (b29g1024.drv), 640x400 = B29G640.DRV (b29g640.drv); each Enable()s its own mode, all three ValidateMode() every mode
cd "$(dirname "$0")/.."
bash tools/build_drv.sh 1280 800 driver/B29GFX.DRV && bash tools/build_drv.sh 1024 640 driver/B29G1024.DRV && bash tools/build_drv.sh 640 400 driver/B29G640.DRV
python3 -I tools/gen_gdiinfo.py 1280 800 > /dev/null   # leave the default include as the 1280x800 one
ls -la driver/*.DRV
