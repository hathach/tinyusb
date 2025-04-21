'use strict';

/// Web Serial API Implementation
class SerialPort {
  constructor(port) {
    this.port = port;
    this.reader = null;
    this.writer = null;
    this.readableStreamClosed = null;
    this.isConnected = false;
    this.readLoop = null;
    this.initialized = false;
  }

  /// Connect and start reading loop
  async connect(options = { baudRate: 9600 }) {
    if (this.initialized) {
      return;
    }
    this.initialized = true;
    await this.port.open(options);

    this.readableStreamClosed = this.port.readable;
    this.reader = this.port.readable.getReader();

    this.writer = this.port.writable.getWriter();
    this.isConnected = true;
    this.readLoop = this._readLoop();
  }

  /// Internal continuous read loop
  async _readLoop() {
    while (this.isConnected) {
      try {
        const { value, done } = await this.reader.read();
        if (done || !this.isConnected) break;
        if (value && this.onReceive) this.onReceive(value);
      } catch (error) {
        this.isConnected = false;
        if (this.onReceiveError) this.onReceiveError(error);
      }
    }
  }

  async _waitForReadLoopToFinish() {
    if (this.readLoop) {
      try {
        await this.readLoop;
      } catch (error) {}
      this.readLoop = null;
    }
  }

  /// Stop reading and release port
  async disconnect() {
    this.isConnected = false;
    await this._waitForReadLoopToFinish();

    if (this.reader) {
      try {
        await this.reader.cancel();
      } catch (error) {}
      this.reader.releaseLock();
    }

    if (this.writer) {
      try {
        await this.writer.close();
      } catch (error) {}
    }

    if (this.readableStreamClosed) {
      try {
        await this.readableStreamClosed;
      } catch (error) {}
    }

    try {
      await this.port.close();
    } catch (error) {}
  }

  /// Send data to port
  send(data) {
    if (!this.writer) throw new Error('Port not connected');
    const encoder = new TextEncoder();
    return this.writer.write(encoder.encode(data));
  }

  async forgetDevice() {}
}

/// WebUSB Implementation
class WebUsbSerialPort {
  constructor(device) {
    this.device = device;
    this.interfaceNumber = 0;
    this.endpointIn = 0;
    this.endpointOut = 0;
    this.isConnected = false;
    this.readLoop = null;
    this.initialized = false;
  }

  isSameDevice(device) {
    return this.device.vendorId === device.vendorId && this.device.productId === device.productId;
  }

  /// Connect and start reading loop
  async connect() {
    if (this.initialized) {
      const devices = await serial.getWebUsbSerialPorts();
      const device = devices.find(d => this.isSameDevice(d.device));
      if (device) {
        this.device = device.device;
      } else {
        return false;
      }
      await this.device.open();
    }
    this.initialized = true;
    await this.device.open();
    try {
      await this.device.reset();
    } catch (error) { }

    if (!this.device.configuration) {
      await this.device.selectConfiguration(1);
    }

    // Find interface with vendor-specific class (0xFF) and endpoints
    for (const iface of this.device.configuration.interfaces) {
      for (const alternate of iface.alternates) {
        if (alternate.interfaceClass === 0xff) {
          this.interfaceNumber = iface.interfaceNumber;
          for (const endpoint of alternate.endpoints) {
            if (endpoint.direction === 'out') this.endpointOut = endpoint.endpointNumber;
            else if (endpoint.direction === 'in') this.endpointIn = endpoint.endpointNumber;
          }
        }
      }
    }

    if (this.interfaceNumber === undefined) {
      throw new Error('No suitable interface found.');
    }

    await this.device.claimInterface(this.interfaceNumber);
    await this.device.selectAlternateInterface(this.interfaceNumber, 0);

    // Set device to ENABLE (0x22 = SET_CONTROL_LINE_STATE, value 0x01 = activate)
    await this.device.controlTransferOut({
      requestType: 'class',
      recipient: 'interface',
      request: 0x22,
      value: 0x01,
      index: this.interfaceNumber,
    });

    this.isConnected = true;
    this.readLoop = this._readLoop();
  }

  async _waitForReadLoopToFinish() {
    if (this.readLoop) {
      try {
        await this.readLoop;
      } catch (error) {}
      this.readLoop = null;
    }
  }

  /// Internal continuous read loop
  async _readLoop() {
    while (this.isConnected) {
      try {
        const result = await this.device.transferIn(this.endpointIn, 16384);
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
  }

  /// Stop reading and release device
  async disconnect() {
    this.isConnected = false;
    await this._waitForReadLoopToFinish();
    try {
      await this.device.controlTransferOut({
        requestType: 'class',
        recipient: 'interface',
        request: 0x22,
        value: 0x00,
        index: this.interfaceNumber,
      });
    } catch (error) {
      console.log(error);
    }
    await this.device.close();
  }

  /// Send data to device
  send(data) {
    return this.device.transferOut(this.endpointOut, data);
  }

  async forgetDevice() {
    await this.disconnect();
    await this.device.forget();
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
