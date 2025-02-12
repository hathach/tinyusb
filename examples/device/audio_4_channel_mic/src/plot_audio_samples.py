#!/usr/bin/env python3
import sounddevice as sd
import matplotlib.pyplot as plt
import numpy as np
import platform

if __name__ == '__main__':

    # If you got "ValueError: No input device matching", that is because your PC name example device
    # differently from tested list below. Uncomment the next line to see full list and try to pick correct one
    # print(sd.query_devices())

    fs = 48000  		# Sample rate
    duration = 1   # Duration of recording

    if platform.system() == 'Windows':
        # WDM-KS is needed since there are more than one MicNode device APIs (at least in Windows)
        device = 'Microphone (MicNode_4_Ch), Windows WASAPI'
    elif platform.system() == 'Darwin':
        device = 'MicNode_4_Ch'
    else:
        device ='default'

    myrecording = sd.rec(int(duration * fs), samplerate=fs, channels=4, dtype='int16', device=device)
    print('Waiting...')
    sd.wait()  # Wait until recording is finished
    print('Done!')


    time = np.arange(0, duration, 1 / fs)  # time vector
    # strip starting zero

    plt.plot(time, myrecording)
    plt.xlabel('Time [s]')
    plt.ylabel('Amplitude')
    plt.title('MicNode 4 Channel')
    plt.legend(['CH-1', 'CH-2', 'CH-3','CH-4'])
    plt.show()
