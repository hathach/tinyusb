#!/usr/bin/env python3
import sounddevice as sd
import matplotlib.pyplot as plt
import numpy as np
import platform


def find_windows_input_device(name_hint, channels, preferred_apis=None):
    """Pick a Windows input device index from query_devices() output."""
    preferred_apis = preferred_apis or []
    name_hint = name_hint.lower()
    candidates = []

    for index in range(len(sd.query_devices())):
        device_info = sd.query_devices(index)
        max_input_channels = int(device_info.get('max_input_channels', 0))

        if max_input_channels < channels:
            continue

        device_name = str(device_info.get('name', '')).lower()
        if name_hint not in device_name:
            continue

        score = 0

        # Prefer exact channel matches to avoid selecting a different stream layout.
        if max_input_channels == channels:
            score += 100
        else:
            score += 10

        hostapi_index = int(device_info.get('hostapi', -1))
        api_name = ''
        if hostapi_index >= 0:
            api_name = str(sd.query_hostapis(hostapi_index)).lower()

        for priority, api_hint in enumerate(preferred_apis):
            if api_hint in api_name:
                score += 50 - priority
                break

        candidates.append((score, index))

    if not candidates:
        raise ValueError(
            'No input device matching hint="{}" with at least {} channels'.format(name_hint, channels)
        )

    return max(candidates, key=lambda item: item[0])[1]

if __name__ == '__main__':

    # If you got "ValueError: No input device matching", that is because your PC name example device
    # differently from tested list below. Uncomment the next line to see full list and try to pick correct one
    # print(sd.query_devices())

    fs = 48000  		# Sample rate
    duration = 1   # Duration of recording

    if platform.system() == 'Windows':
        # Match by substring to support names like "Microphone (2- MicNode_4_Ch)".
        device = find_windows_input_device('micnode_4_ch', channels=4, preferred_apis=['wasapi', 'wdm-ks', 'mme'])
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
