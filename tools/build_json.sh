#!/bin/bash
# build structure definition for GCS hal interface from dwarf debug information in native executable.
# Needs dwarf_to_c (https://www.github.com/laanwj/dwarf_to_c)
extract_structures_json.py ../native/egl/cube _gcsHAL_INTERFACE > data/gcs_hal_interface.json
