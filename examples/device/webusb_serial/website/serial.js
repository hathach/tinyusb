'use strict';

/// Web Serial API Implementation
/// https://developer.mozilla.org/en-US/docs/Web/API/SerialPort
class SerialPort {
  constructor(port) {
    this._port = port;
    this._readLoopPromise = null;
    this._reader = null;
    this._writer = null;
    this._initialized = false;
    this._keepReading = true;
    this.isConnected = false;
  }

  /// Connect and start reading loop
  async connect(options = { baudRate: 9600 }) {
    if (this._initialized) {
      try {
        await this.disconnect();
      } catch (error) {
        console.error('Error disconnecting previous port:', error);
      }

      if (this._readLoopPromise) {
        try {
          await this._readLoopPromise;
        } catch (error) {
          console.error('Error in read loop:', error);
        }
      }
      this._readLoopPromise = null;
    }
    this._initialized = true;

    this.isConnected = true;
    this._keepReading = true;

    try {
      await this._port.open(options);
    } catch (error) {
      this.isConnected = false;
      throw error;
    }

    this._readLoopPromise = this._readLoop();
  }

  /// Internal continuous read loop
  async _readLoop() {
    try {
      while (this._port.readable && this._keepReading) {
        this._reader = this._port.readable.getReader();
        try {
          while (true) {
            const { value, done } = await this._reader.read();
            if (done) {
              // |reader| has been canceled.
              break;
            }
            if (this.onReceive) {
              this.onReceive(value);
            }
          }
        } catch (error) {
          if (this.onReceiveError) this.onReceiveError(error);
        } finally {
          this._reader.releaseLock();
        }
      }
    } finally {
      this.isConnected = false;
      await this._port.close();
    }
  }

  /// Stop reading and release port
  async disconnect() {
    this._keepReading = false;

    if (this._reader) {
      try {
        await this._reader.cancel();
      } catch (error) {
        console.error('Error cancelling reader:', error);
      }
      this._reader.releaseLock();
    }

    if (this._writer) {
      try {
        await this._writer.abort();
      } catch (error) {
        console.error('Error closing writer:', error);
      }
      this._writer.releaseLock();
    }

    try {
      await this._port.close();
    } catch (error) {
      console.error('Error closing port:', error);
    }

    if (this._readLoopPromise) {
      try {
        await this._readLoopPromise;
      } catch (error) {
        console.error('Error in read loop:', error);
      }
    }
  }

  /// Send data to port
  send(data) {
    if (!this._port.writable) {
      throw new Error('Port is not writable');
    }
    this._writer = this._port.writable.getWriter();
    if (!this._writer) {
      throw new Error('Failed to get writer from port');
    }
    try {
      return this._writer.write(data);
    } finally {
      this._writer.releaseLock();
    }
  }

  async forgetDevice() {}
}

/// WebUSB Implementation
class WebUsbSerialPort {
  constructor(device) {
    this._device = device;
    this._interfaceNumber = 0;
    this._endpointIn = 0;
    this._endpointOut = 0;
    this.isConnected = false;
    this._readLoopPromise = null;
    this._initialized = false;
    this._keepReading = true;

    this._vendorId = device.vendorId;
    this._productId = device.productId;
  }

  _isSameWebUsbSerialPort(webUsbSerialPort) {
    return this._vendorId === webUsbSerialPort._vendorId && this._productId === webUsbSerialPort._productId;
  }

  /// Connect and start reading loop
  async connect() {
    if (this._initialized) {
      try {
        await this.disconnect();
      } catch (error) {
        console.error('Error disconnecting previous device:', error);
      }

      const webUsbSerialPorts = await serial.getWebUsbSerialPorts();
      const webUsbSerialPort = webUsbSerialPorts.find(serialPort => this._isSameWebUsbSerialPort(serialPort));
      this._device = webUsbSerialPort ? webUsbSerialPort._device : this._device;
    }
    this._initialized = true;

    this.isConnected = true;
    this._keepReading = true;
    try {
      await this._device.open();

      if (!this._device.configuration) {
        await this._device.selectConfiguration(1);
      }

      // Find interface with vendor-specific class (0xFF) and endpoints
      for (const iface of this._device.configuration.interfaces) {
        for (const alternate of iface.alternates) {
          if (alternate.interfaceClass === 0xff) {
            this._interfaceNumber = iface.interfaceNumber;
            for (const endpoint of alternate.endpoints) {
              if (endpoint.direction === 'out') this._endpointOut = endpoint.endpointNumber;
              else if (endpoint.direction === 'in') this._endpointIn = endpoint.endpointNumber;
            }
          }
        }
      }

      if (this._interfaceNumber === undefined) {
        throw new Error('No suitable interface found.');
      }

      await this._device.claimInterface(this._interfaceNumber);
      await this._device.selectAlternateInterface(this._interfaceNumber, 0);

      // Set device to ENABLE (0x22 = SET_CONTROL_LINE_STATE, value 0x01 = activate)
      await this._device.controlTransferOut({
        requestType: 'class',
        recipient: 'interface',
        request: 0x22,
        value: 0x01,
        index: this._interfaceNumber,
      });
    } catch (error) {
      this.isConnected = false;
      throw error;
    }

    this._readLoopPromise = this._readLoop();
  }

  /// Internal continuous read loop
  async _readLoop() {
    try {
      while (this._keepReading && this.isConnected) {
        try {
          const result = await this._device.transferIn(this._endpointIn, 16384);
          if (result.data && this.onReceive) {
            this.onReceive(result.data);
          }
        } catch (error) {
          this.isConnected = false;
          if (this.onReceiveError) {
            this.onReceiveError(error);
          }
        }
      }
    } finally {
      this.isConnected = false;
      await this._device.close();
    }
  }

  /// Stop reading and release device
  async disconnect() {
    this._keepReading = false;

    try {
      await this._device.controlTransferOut({
        requestType: 'class',
        recipient: 'interface',
        request: 0x22,
        value: 0x00,
        index: this._interfaceNumber,
      });
    } catch (error) {
      console.error('Error sending control transfer:', error);
    }

    await this._device.releaseInterface(this._interfaceNumber);

    if (this._readLoopPromise) {
      try {
        await this._readLoopPromise;
      } catch (error) {
        console.error('Error in read loop:', error);
      }
    }
  }

  /// Send data to device
  send(data) {
    return this._device.transferOut(this._endpointOut, data);
  }

  async forgetDevice() {
    await this.disconnect();
    await this._device.forget();
  }
}

// Utility Functions
const serial = {
  isWebSerialSupported: () => 'serial' in navigator,
  isWebUsbSupported: () => 'usb' in navigator,

  async getSerialPorts() {
    if (!this.isWebSerialSupported()) return [];
    const ports = await navigator.serial.getPorts();
    return ports.map(port => new SerialPort(port));
  },

  async getWebUsbSerialPorts() {
    if (!this.isWebUsbSupported()) return [];
    const devices = await navigator.usb.getDevices();
    return devices.map(device => new WebUsbSerialPort(device));
  },

  async requestSerialPort() {
    const port = await navigator.serial.requestPort();
    return new SerialPort(port);
  },

  async requestWebUsbSerialPort() {
    const filters = [
      { vendorId: 0xcafe }, // TinyUSB
      { vendorId: 0x239a }, // Adafruit
      { vendorId: 0x2e8a }, // Raspberry Pi
      { vendorId: 0x303a }, // Espressif
      { vendorId: 0x2341 }, // Arduino
    ];
    const device = await navigator.usb.requestDevice({ filters });
    return new WebUsbSerialPort(device);
  }
};
