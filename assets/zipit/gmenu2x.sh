#!/bin/sh

#source /etc/profile
trap "" hup
#clear

#cd /mnt/sd0/gmenu2x
#exec /mnt/sd0/gmenu2x/gmenu2x

export SDL_AUDIODRIVER=dsp 
export SDL_NOMOUSE=1 
export SDL_VIDEO_FBCON_ROTATION="CCW" 
./gmenu2x
