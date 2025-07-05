'use strict';

const serial = {
  isWebUsbSupported: () => 'usb' in navigator,

  // Returns array of connected devices wrapped as serial.Port instances
  async getPorts() {
    const devices = await navigator.usb.getDevices();
    return devices.map(device => new serial.Port(device));
  },

  // Prompts user to select a device matching filters and wraps it in serial.Port
  async requestPort() {
    const filters = [
      { vendorId: 0xcafe }, // TinyUSB
      { vendorId: 0x239a }, // Adafruit
      { vendorId: 0x2e8a }, // Raspberry Pi
      { vendorId: 0x303a }, // Espressif
      { vendorId: 0x2341 }, // Arduino
    ];
    const device = await navigator.usb.requestDevice({ filters });
    return new serial.Port(device);
  },

  Port: class {
    constructor(device) {
      this.device = device;
      this.interfaceNumber = 0;
      this.endpointIn = 0;
      this.endpointOut = 0;
      this.readLoopActive = false;
    }

    portPointToSameDevice(port) {
      if (this.device.vendorId !== port.device.vendorId) return false;
      if (this.device.productId !== port.device.productId) return false;
      if (this.device.serialNumber !== port.device.serialNumber) return false;
      return true;
    }

    // Connect and start reading loop
    async connect() {
      await this.device.open();

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

      this.readLoopActive = true;
      this._readLoop();
    }

    // Internal continuous read loop
    async _readLoop() {
      while (this.readLoopActive) {
        try {
          const result = await this.device.transferIn(this.endpointIn, 64);
          if (result.data && this.onReceive) {
            this.onReceive(result.data);
          }
        } catch (error) {
          this.readLoopActive = false;
          if (this.onReceiveError) {
            this.onReceiveError(error);
          }
        }
      }
    }

    // Stop reading and release device
    async disconnect() {
      this.readLoopActive = false;
      await this.device.controlTransferOut({
        requestType: 'class',
        recipient: 'interface',
        request: 0x22,
        value: 0x00,
        index: this.interfaceNumber,
      });
      await this.device.close();
    }

    // Send data to device
    send(data) {
      return this.device.transferOut(this.endpointOut, data);
    }

    async forgetDevice() {
      if (this.device.opened) {
        await this.device.close();
      }
      await this.device.forget();
    }
  }
};
