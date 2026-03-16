#!/bin/sh

mkdir /tmp/
mount -t tmpfs /tmp /tmp
mkdir /comm/
mount -t tmpfs /comm /comm
mkdir /system/
mount -t systemfs /system/ /system/