#!/usr/bin/env python

import serial
import pygame
import logging
from random import randint

soundDir = '/home/pi/workspace/mission_control/sounds'

baudRate = 2000000

# setup logging
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s %(levelname)s %(message)s',
    filename='/var/log/mission-control.log',
    filemode='w')

logging.info('Starting mission control panel processing...')
print 'Starting mission control panel processing...'

# User events
END_FLUSH_EVENT = pygame.USEREVENT + 0
END_MESSAGE_EVENT = pygame.USEREVENT + 1

# init pygame
pygame.mixer.pre_init(44100, -16, 10, 4096)
pygame.init()

# Load sounds...
toilet = pygame.mixer.Sound(soundDir + '/mr_thirsty.wav')
rcs = pygame.mixer.Sound(soundDir + '/rcs_sound.wav')
thrust = pygame.mixer.Sound(soundDir + '/thrusters2.wav')
alarm = pygame.mixer.Sound(soundDir + '/master_alarm.wav')
launch = pygame.mixer.Sound(soundDir + '/liftoff_commentary.wav')
pressurize = pygame.mixer.Sound(soundDir + '/compression.wav')
chatters = []
for i in range(10, 51):
    x = '%s/chatter_%s.wav' % (soundDir, i)
    chatters.append(pygame.mixer.Sound(x))
    logging.debug('Adding in message file %s', x)
    print 'Adding in message file %s' % (x)

backgroundChannel = pygame.mixer.music.load(
    soundDir + '/background_sound.wav')

flushChannel = pygame.mixer.Channel(0)
rcsChannel = pygame.mixer.Channel(1)
thrustChannel = pygame.mixer.Channel(2)
alarmChannel = pygame.mixer.Channel(3)
messageChannel = pygame.mixer.Channel(4)
cryoChannel = pygame.mixer.Channel(5)
launchChannel = pygame.mixer.Channel(6)

flushChannel.set_endevent(END_FLUSH_EVENT)
messageChannel.set_endevent(END_MESSAGE_EVENT)

rcs.set_volume(0.4)
alarm.set_volume(0.25)

running = True
chatterIndex = 0

logging.info('Starting main loop...')

# need to see if we can bind this programatically, not hard coded... will look into
# adding a udev rule to accomplish this
ser = serial.Serial('/dev/ttyACM0', baudRate)

while running:

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            logging.info('Received exit - quitting script')
            running = False
            ser.close()
            pygame.quit()
        if event.type == END_FLUSH_EVENT:
            ser.write('FLUSH_COMPLETE\n')
            logging.debug('To serial: FLUSH_COMPLETE')
        if event.type == END_MESSAGE_EVENT:
            ser.write('MESSAGE_COMPLETE\n')
            logging.debug('To serial: MESSAGE_COMPLETE')

    action = ser.readline()[:-2]
    if action:
        logging.debug('From serial: %s', action)
        if action == 'FLUSH':
            flushChannel.play(toilet)
        if action == 'RCS:ON':
            rcsChannel.play(rcs, -1)
        if action == 'RCS:OFF':
            rcs.stop()
        if action == 'THRUST:ON':
            thrustChannel.play(thrust, -1)
        if action == 'THRUST:OFF':
            thrust.stop()
        if action == 'MASTER_ALARM:ON':
            alarmChannel.play(alarm, -1)
        if action == 'MASTER_ALARM:OFF':
            alarm.stop()
        if action == 'LAUNCH':
            launchChannel.play(launch)
        if action == 'ABORT':
            launch.stop()
        if action == 'MESSAGE':
            messageChannel.play(chatters[chatterIndex])
            chatterIndex += 1
            if chatterIndex >= len(chatters):
                chatterIndex = 0

    clock.tick(1)

pygame.quit()

logging.info('Mission control panel has ended')
