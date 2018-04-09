#!/usr/bin/env python

import serial
import pygame
from random import randint

END_FLUSH_EVENT = pygame.USEREVENT + 0
END_MESSAGE_EVENT = pygame.USEREVENT + 1

pygame.mixer.pre_init(44100, -16, 10, 4096)
pygame.init()

toilet = pygame.mixer.Sound('/home/pi/sounds/mr_thirsty.wav')
rcs_l = pygame.mixer.Sound('/home/pi/sounds/rcs_sound.wav')
thrust = pygame.mixer.Sound('/home/pi/sounds/thrusters2.wav')
alarm = pygame.mixer.Sound('/home/pi/sounds/master_alarm.wav')
launch = pygame.mixer.Sound('/home/pi/sounds/liftoff_commentary.wav')

chatters = []
for i in range(10, 51):
    x = "/home/pi/sounds/chatter_%s.wav" % (i)
    chatters.append(pygame.mixer.Sound(x))

rcs_l.set_volume(0.5)
alarm.set_volume(0.25)

c0 = pygame.mixer.Channel(0)
c0.set_endevent(END_FLUSH_EVENT)
c1 = pygame.mixer.Channel(1)
c2 = pygame.mixer.Channel(2)
c3 = pygame.mixer.Channel(3)
c4 = pygame.mixer.Channel(4)
c4.set_endevent(END_MESSAGE_EVENT)

ser = serial.Serial('/dev/ttyACM0', 9600, parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE, bytesize=serial.EIGHTBITS, timeout=1)

running = True

while running:
        
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        if event.type == END_FLUSH_EVENT:
            ser.write('FLUSH_COMPLETE\n')
        if event.type == END_MESSAGE_EVENT:
            ser.write('MESSAGE_COMPLETE\n')
    
    read_serial = ser.readline()[:-2]
    if read_serial:
        if read_serial == 'FLUSH':
            c0.play(toilet)
        if read_serial == 'RCS_LEFT:ON':
            c1.play(rcs_l, -1)
        if read_serial == 'RCS_LEFT:OFF':
            rcs_l.stop()
        if read_serial == 'RCS_RIGHT:ON':
            c1.play(rcs_l, -1)
        if read_serial == 'RCS_RIGHT:OFF':
            rcs_l.stop()
        if read_serial == 'RCS_CENTER:ON':
            c1.play(rcs_l, -1)
        if read_serial == 'RCS_CENTER:OFF':
            rcs_l.stop()
        if read_serial == 'THRUST_LEFT:ON':
            c2.play(thrust, -1)
        if read_serial == 'THRUST_LEFT:OFF':
            thrust.stop()
        if read_serial == 'THRUST_RIGHT:ON':
            c2.play(thrust, -1)
        if read_serial == 'THRUST_RIGHT:OFF':
            thrust.stop()
        if read_serial == 'THRUST_CENTER:ON':
            c2.play(thrust, -1)
        if read_serial == 'THRUST_CENTER:OFF':
            thrust.stop()
        if read_serial == 'MASTER_ALARM:ON':
            c3.play(alarm, -1)
        if read_serial == 'MASTER_ALARM:OFF':
            alarm.stop()
        if read_serial == 'LAUNCH':
            c0.play(launch)
        if read_serial == 'MESSAGE':
            c4.play(chatters[randint(0, 40)])

pygame.quit()
