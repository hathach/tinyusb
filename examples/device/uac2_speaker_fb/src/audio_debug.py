# Install python3 HID package https://pypi.org/project/hid/
# Install python3 matplotlib package https://pypi.org/project/matplotlib/

from ctypes import *
try:
    import hid
    import matplotlib.pyplot as plt
    import matplotlib.animation as animation
except:
    print("Missing import, please try 'pip install hid matplotlib' or consult your OS's python package manager.")

# Example must be compiled with CFG_AUDIO_DEBUG=1
VID = 0xcafe
PID = 0x4014

CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX = 2

class audio_debug_info_t (Structure):
    _fields_ = [("sample_rate", c_uint32),
                ("alt_settings", c_uint8),
                ("mute", (CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1) * c_int8),
                ("volume", (CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1) * c_int16),
                ("fifo_size", c_uint16),
                ("fifo_count", c_uint16),
                ("fifo_count_avg", c_uint16)
                ]

dev = hid.Device(VID, PID)

if dev:
    # Create figure for plotting
    fig = plt.figure()
    ax = fig.add_subplot(1, 1, 1)
    fifo_avg = []
    fifo_cnt = []
    # This function is called periodically from FuncAnimation
    def animate(i):
        info = None
        for i in range(30):
            try:
                str_in = dev.read(64, 50)
                info = audio_debug_info_t.from_buffer_copy(str_in)

                global fifo_avg
                global fifo_cnt
                fifo_avg.append(info.fifo_count_avg)
                fifo_cnt.append(info.fifo_count)
            except:
                exit(1)
        # Limit to 1000 items
        fifo_avg = fifo_avg[-1000:]
        fifo_cnt = fifo_cnt[-1000:]

        if info is not None:
            # Draw x and y lists
            ax.clear()
            ax.plot(fifo_cnt, label='FIFO count')
            ax.plot(fifo_avg, label='FIFO average')
            ax.legend()
            ax.set_ylim(bottom=0, top=info.fifo_size)

            # Format plot
            plt.title('FIFO information')
            plt.grid()

            print(f'Sample rate:{info.sample_rate} | Alt settings:{info.alt_settings} | Volume:{info.volume[:]}')

    ani = animation.FuncAnimation(fig, animate, interval=10)
    plt.show()
