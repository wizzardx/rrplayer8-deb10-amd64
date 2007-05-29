#!/bin/bash

# This script will be run under fakeroot to help setup the correct permissions etc
# for the .deb file
set -e

# Set correct ownership to progs/player directory (so player can write log file to it's program files dir):
# - This is an evil setup, eg player binary could alter itself
pwd
chown radman:radman ../data/data/radio_retail/progs/player
