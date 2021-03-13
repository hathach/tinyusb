import sounddevice as sd
import matplotlib.pyplot as plt
import numpy as np

if __name__ == '__main__':

  #  devList = sd.query_devices()
  #  print(devList)

    fs = 48000  # Sample rate
    duration = 100e-3                     # Duration of recording
    device = 'Microphone (MicNode) MME'   # MME is needed since there are more than one MicNode device APIs (at least in Windows)

    myrecording = sd.rec(int(duration * fs), samplerate=fs, channels=1, dtype='int16', device=device)
    print('Waiting...')
    sd.wait()  # Wait until recording is finished
    print('Done!')

    time = np.arange(0, duration, 1 / fs)  # time vector
    plt.plot(time, myrecording)
    plt.xlabel('Time [s]')
    plt.ylabel('Amplitude')
    plt.title('MicNode')
    plt.show()
    