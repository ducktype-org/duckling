#!/bin/bash

parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

make "$@" 2>&1 >/dev/null | exec $parent_path/lib/color_ld_out.py
