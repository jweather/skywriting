# Airborne Lightpainting

Code in progress for my lightpainting rig on an airplane.

More details in the project log here: https://hackaday.io/project/6374-skywriting

Basically, teensy_lightpainter is the flying microcontroller, driving a WS2811 LED strip
from BMP images on an SD card.  It's set up to read servo signals from the RC receiver for control and data.

The control part is a simple on/off switch on the RC transmitter.  The data part is supplied by
the ground station/remote control, which is running digispark_uplink.  It's generating PPM data
via the excellent [TinyPpmGen library by RCNavy](https://digistump.com/board/index.php?topic=1685.0),
which goes via trainer cable to the RC transmitter.

The fun part is using the servo signals to transmit digital data.  I need this process to be
reasonably efficient, since using the trainer function for this means that the pilot turns over
complete control of the plane to the ground station while uplinking.  So without using a nunchuck
or something to fly the plane during uplink, they need to be short bursts.

The datalink protocol uses one channel as an edge-triggered clock, indicating when to measure
signals on the second channel, which takes one of 10 discrete values that can be easily distinguished.
A packet consists of:
 - a sync pulse (1500us pulse width) for 150ms
 - four edge-triggered samples of the data channel 150ms apart, which are treated as decimal digits
 
The data values are:
 <type> <tens> <units> <checksum>
 
Where <type> selects the variable being set, <tens> and <units> make up the value, and <checksum> should
result in the sum of the four digits being 0 (modulo 10).

The data link wound up being much slower than I expected, possibly because the RC transmitter
(or maybe receiver) doesn't like rapid transitions in pulse width, so it takes up to 150ms for the data
channel to settle on the new value.  As a result, I'm not trying to transmit all the variables every
time, but just the variable currently selected on the ground station (which is probably the one you
wanted to change anyway).  I'm sure there's room for improvement in the protocol, but I'm out of time.