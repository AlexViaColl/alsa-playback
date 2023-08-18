#!/bin/sh

set -xe

gcc -Wall -Werror -std=c11 -pedantic -o playback `pkg-config --libs alsa` -lm main.c
