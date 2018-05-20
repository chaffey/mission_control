#!/usr/bin/env python

import serial
import pygame
import logging
from random import randint

SONG_COUNT = 10

soundDir = '/home/pi/workspace/mission_control/sounds'

baudRate = 2000000

# setup logging
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s %(levelname)s %(message)s',
    filename='/var/log/mission-control.log',
    filemode='w')

logging.info('Starting mission control panel processing...')

# User events
END_FLUSH_EVENT = pygame.USEREVENT + 0
END_MESSAGE_EVENT = pygame.USEREVENT + 1

# init pygame
pygame.mixer.pre_init(44100, -16, 16, 4096)
pygame.init()
pygame.mixer.set_num_channels(16);

# Load sounds...
toilet = pygame.mixer.Sound(soundDir + '/mr_thirsty.wav')
rcs = pygame.mixer.Sound(soundDir + '/rcs_sound.wav')
thrust = pygame.mixer.Sound(soundDir + '/thrusters2.wav')
alarm = pygame.mixer.Sound(soundDir + '/master_alarm.wav')
launch = pygame.mixer.Sound(soundDir + '/liftoff_commentary.wav')
pressurize = pygame.mixer.Sound(soundDir + '/compression.wav')
compactor = pygame.mixer.Sound(soundDir + '/compactor.wav')
background = pygame.mixer.Sound(soundDir + '/background_noise.wav')

chatters = []
for i in range(10, 51):
    x = '%s/chatter_%s.wav' % (soundDir, i)
    chatters.append(pygame.mixer.Sound(x))
    logging.debug('Adding in message file %s', x)

flushChannel = pygame.mixer.Channel(0)
rcsChannel = pygame.mixer.Channel(1)
thrustChannel = pygame.mixer.Channel(2)
alarmChannel = pygame.mixer.Channel(3)
messageChannel = pygame.mixer.Channel(4)
cryoChannel = pygame.mixer.Channel(5)
songChannel = pygame.mixer.Channel(6)
crewLifeChannel = pygame.mixer.Channel(7)
launchChannel = pygame.mixer.Channel(8)
backgroundChannel = pygame.mixer.Channel(9)

flushChannel.set_endevent(END_FLUSH_EVENT)
messageChannel.set_endevent(END_MESSAGE_EVENT)

rcs.set_volume(0.4)
alarm.set_volume(0.25)
backgroundChannel.set_volume(0.3)

backgroundChannel.play(background, -1)

running = True
chatterIndex = 0
songIndex = 0
logging.info('Starting main loop...')

# need to see if we can bind this programatically, not hard coded... will look into
# adding a udev rule to accomplish this
ser = serial.Serial('/dev/ttyACM0', baudRate, timeout=0.050)

pygame.mixer.music.load(soundDir + '/background_noise.wav')

while running:
    events = pygame.event.get()
    for event in events:
        print 'Event: %s' % (event.type)
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
        print 'From serial: %s' % (action)
        if action == 'MUSIC:ON':
            s = '%s/song_%d.mp3' % (soundDir, songIndex)
            pygame.mixer.music.load(s)
            pygame.mixer.music.play()
        if action == 'MUSIC:OFF':
            pygame.mixer.music.stop()
        if action == 'MUSIC:CHANGE':
            pygame.mixer.music.stop()
            songIndex += 1
            if songIndex >= SONG_COUNT:
                songIndex = 0
            s = '%s/song_%d.mp3' % (soundDir, songIndex)
            pygame.mixer.music.load(s)
            pygame.mixer.music.play()
        if action == 'SUIT:PRESSURIZE':
            crewLifeChannel.play(pressurize)
        if action == 'TRASH':
            crewLifeChannel.play(compactor)
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

pygame.quit()

logging.info('Mission control panel has ended')
