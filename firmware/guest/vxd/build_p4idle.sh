#!/bin/bash
# compatibility wrapper: build the idle VxD
exec bash "$(dirname "$0")/build_vxd.sh" "$(dirname "$0")/p4idle.S" P4IDLE "$(dirname "$0")/p4idle.vxd"
