#!/bin/sh

# Run the networking startup
if cmdline "--no-dhcp"; then
    exit 0
fi

net-startup
