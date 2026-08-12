# Automatic-Rotating-Platform
Description: The objective of this project is to construct a rotary platform capable of precisely
rotating a loudspeaker or a microphone and to measure the impulse response at each angular
position. The construction will include the following components: a 3D-printed table, an Arduino
or alternative microcontroller, a condenser microphone, a stepper motor, and a stepper motor
driver. The platform must be robust enough to support a loudspeaker weighing up to 20 kg and
should offer various mounting options, such as stand-mount configurations and different sizes for
the rotating table. The impulse response will be measured with the exponential sweep method. The
rotation of the platform will be dictated by the sound, with a data-over-sound algorithm (you will
have to test a series of algorithms such as Goertzel, DTMF, ASK, PSK, FSK and find the one that
is most robust). The data will be played in the silence parts between the test signals. The data will
contain information such as: the relative angle of rotation, the speed of rotation, the direction of
rotation, and so on.

Goal: Build the automatically rotating platform. The final result should work as follows: we place
the loudspeaker on the platform and we play only one audio file that contains all the information,
the test signals for the impulse response measurement and the data-over-sound that rotates the
platform. The recorded signal is post-processed to obtain the final polar pattern of the loudspeaker
or microphone.

References:
- Cristian Negrescu, Victor Popa, „Măsurarea și caracterizarea sistemelor acustice: îndrumar
de laborator,” 2013.
- Angelo Farina, „Advancements in impulse response measurements by sine sweeps,”
Vienna, May 2007.
