#!/bin/sh

# If user doesnt want it, user doesnt want it
if cmdline "--no-audio-server"; then
    exit 0
fi

# Daemonize symphonyd
symphonyd --daemon
