#!/bin/sh

# Converts a target to an actual architecture
if echo "$1" | grep -Eq 'i[[:digit:]]86-'; then
    echo i386
elif echo "$1" | grep -Eq 'x86_64-'; then
    echo x86_64
elif echo "$1" | grep -Eq 'aarch64-'; then
    echo aarch64
else
    echo "$1" | grep -Eo '^[[:alnum:]_]*'
fi