#!/bin/bash
# Clear all snap-related environment variables

# Unset all SNAP variables
for var in $(env | grep -o "^[A-Z_]*SNAP[A-Z_]*"); do
    unset $var
done

# Clear library paths that might point to snap
unset LD_LIBRARY_PATH
unset LD_PRELOAD
unset GIO_MODULE_DIR  
unset LOCPATH
unset GTK_PATH
unset GTK_EXE_PREFIX
unset GSETTINGS_SCHEMA_DIR

# Run QEMU with clean environment
exec /usr/bin/qemu-system-x86_64 "$@"
