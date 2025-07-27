#!/bin/sh

BUILDSCRIPTS_ROOT=${BUILDSCRIPTS_ROOT:-"$(cd `dirname $0` && pwd)"}
PROJECT_ROOT=${PROJECT_ROOT:"${BUILDSCRIPTS_ROOT}/.."}

# Get config
. $BUILDSCRIPTS_ROOT/config.sh

# Create base directories
mkdir $SYSROOT/device/ || true
mkdir $SYSROOT/comm/ || true
mkdir $SYSROOT/tmp/ || true
mkdir $SYSROOT/kernel/ || true