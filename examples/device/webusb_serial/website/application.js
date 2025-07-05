'use strict';

(async () => {
  // bind to the html
  const connectWebUsbSerialBtn = document.getElementById('connect_webusb_serial_btn');
  const connectSerialBtn = document.getElementById('connect_serial_btn');
  const disconnectBtn = document.getElementById('disconnect_btn');

  const newlineModeSelect = document.getElementById('newline_mode_select');
  const autoReconnectCheckbox = document.getElementById('auto_reconnect_checkbox');
  const forgetDeviceBtn = document.getElementById('forget_device_btn');
  const forgetAllDevicesBtn = document.getElementById('forget_all_devices_btn');
  const resetAllBtn = document.getElementById('reset_all_btn');
  const resetOutputBtn = document.getElementById('reset_output_btn');
  const copyOutputBtn = document.getElementById('copy_output_btn');

  const statusSpan = document.getElementById('status_span');

  const commandHistoryScrollbox = document.getElementById('command_history_scrollbox');
  const commandLineInput = document.getElementById('command_line_input');
  const sendModeBtn = document.getElementById('send_mode_btn');

  const receivedDataScrollbox = document.getElementById('received_data_scrollbox');

  const nearTheBottomThreshold = 100; // pixels from the bottom to trigger scroll

  class Application {
    constructor() {
      this.currentPort = null;
      this.textEncoder = new TextEncoder();
      this.textDecoder = new TextDecoder();

      this.reconnectTimeoutId = null;

      this.commandHistory = [];
      this.commandHistoryIndex = -1;
      this.lastCommandCount = 0;
      this.lastCommand = null;
      this.lastCommandBtn = null;

      // bind the UI elements
      connectWebUsbSerialBtn.addEventListener('click', () => this.connectWebUsbSerialPort());
      connectSerialBtn.addEventListener('click', () => this.connectSerialPort());
      disconnectBtn.addEventListener('click', () => this.disconnectPort());
      newlineModeSelect.addEventListener('change', () => this.setNewlineMode());
      autoReconnectCheckbox.addEventListener('change', () => this.autoReconnectChanged());
      forgetDeviceBtn.addEventListener('click', () => this.forgetPort());
      forgetAllDevicesBtn.addEventListener('click', () => this.forgetAllPorts());
      resetAllBtn.addEventListener('click', () => this.resetAll());
      resetOutputBtn.addEventListener('click', () => this.resetOutput());
      copyOutputBtn.addEventListener('click', () => this.copyOutput());
      commandLineInput.addEventListener('keydown', (e) => this.handleCommandLineInput(e));
      sendModeBtn.addEventListener('click', () => this.toggleSendMode());

      // restore state from localStorage

      // Restore command history
      let savedCommandHistory = JSON.parse(localStorage.getItem('commandHistory') || '[]');
      for (const cmd of savedCommandHistory) {
        this.appendCommandToHistory(cmd);
      }

      this.sendMode = localStorage.getItem('sendMode') || 'command';
      this.setSendMode(this.sendMode);

      autoReconnectCheckbox.checked = localStorage.getItem('autoReconnect') === 'true';

      let savedNewlineMode = localStorage.getItem('newlineMode');
      if (savedNewlineMode) {
        newlineModeSelect.value = savedNewlineMode;
      }

      this.connectWebUsbSerialPort(true);
    }

    appendCommandToHistory(text) {
      if (text === this.lastCommand) {
        // Increment count and update button
        this.lastCommandCount++;
        this.lastCommandBtn.textContent = `${text} ×${this.lastCommandCount}`;
      } else {
        // Add a new entry to the command history
        this.commandHistory.push(text);
        localStorage.setItem('commandHistory', JSON.stringify(this.commandHistory));
        this.commandHistoryIndex = -1;

        const commandHistoryEntryBtn = document.createElement('button');
        commandHistoryEntryBtn.className = 'command-history-entry';
        commandHistoryEntryBtn.type = 'button';
        commandHistoryEntryBtn.textContent = text;
        commandHistoryEntryBtn.addEventListener('click', () => {
          if (commandLineInput.disabled) return;
          commandLineInput.value = text;
          commandLineInput.focus();
        });
        commandHistoryScrollbox.appendChild(commandHistoryEntryBtn);

        this.lastCommand = text;
        this.lastCommandBtn = commandHistoryEntryBtn;

        // Scroll to the new entry if near the bottom
        const distanceFromBottom = commandHistoryScrollbox.scrollHeight - (commandHistoryScrollbox.scrollTop + commandHistoryScrollbox.clientHeight);
        if (distanceFromBottom < nearTheBottomThreshold) {
          requestAnimationFrame(() => {
            commandHistoryEntryBtn.scrollIntoView({ behavior: 'instant' });
          });
        }
      }
    }

    appendLineToReceived(text) {
      const div = document.createElement('div');
      div.textContent = text;
      receivedDataScrollbox.appendChild(div);

      // Scroll to the new entry if near the bottom
      const distanceFromBottom = receivedDataScrollbox.scrollHeight - (receivedDataScrollbox.scrollTop + receivedDataScrollbox.clientHeight);
      if (distanceFromBottom < nearTheBottomThreshold) {
        requestAnimationFrame(() => {
          div.scrollIntoView({ behavior: 'instant' });
        });
      }
    }

    setStatus(msg, level = 'info') {
      console.log(msg);
      statusSpan.textContent = msg;
      statusSpan.className = 'status status-' + level;
    }

    updateUIConnectionState() {
      if (this.currentPort && this.currentPort.isConnected) {
        connectWebUsbSerialBtn.style.display = 'none';
        connectSerialBtn.style.display = 'none';
        disconnectBtn.style.display = 'block';
        commandLineInput.disabled = false;
        commandLineInput.focus();
      } else {
        if (serial.isWebUsbSupported()) {
          connectWebUsbSerialBtn.style.display = 'block';
        }
        if (serial.isWebSerialSupported()) {
          connectSerialBtn.style.display = 'block';
        }
        if (!serial.isWebUsbSupported() && !serial.isWebSerialSupported()) {
          this.setStatus('Your browser does not support WebUSB or WebSerial', 'error');
        }
        disconnectBtn.style.display = 'none';
        commandLineInput.disabled = true;
        commandLineInput.value = '';
        commandLineInput.blur();
      }
    }

    async disconnectPort() {
      this.stopAutoReconnect();
      if (!this.currentPort) return;

      try {
        await this.currentPort.disconnect();
      }
      catch (error) {
        this.setStatus(`Disconnect error: ${error.message}`, 'error');
      }

      this.updateUIConnectionState();
    }

    async onReceive(dataView) {
      this.updateUIConnectionState();
      let text = this.textDecoder.decode(dataView);
      text = this.normalizeNewlines(text);
      this.appendLineToReceived(text);
    }

    async onReceiveError(error) {
      this.setStatus(`Read error: ${error.message}`, 'error');
      await this.disconnectPort();
      // Start auto reconnect on error if enabled
      this.tryAutoReconnect();
    }

    async connectSerialPort() {
      if (!serial.isWebSerialSupported()) {
        this.setStatus('Serial not supported on this browser', 'error');
        return;
      }
      try {
        this.setStatus('Requesting device...', 'info');
        this.currentPort = await serial.requestSerialPort();
        this.currentPort.onReceiveError = error => this.onReceiveError(error);
        this.currentPort.onReceive = dataView => this.onReceive(dataView);
        await this.currentPort.connect();
        this.setStatus('Connected', 'info');
      } catch (error) {
        this.setStatus(`Connection failed: ${error.message}`, 'error');
        if (this.currentPort) {
          await this.currentPort.forgetDevice();
          this.currentPort = null;
        }
      }
    }

    async connectWebUsbSerialPort(initial = false) {
      if (!serial.isWebUsbSupported()) {
        this.setStatus('WebUSB not supported on this browser', 'error');
        return;
      }
      try {
        let first_time_connection = false;
        let grantedDevices = await serial.getWebUsbSerialPorts();
        if (initial) {
          if (!autoReconnectCheckbox.checked || grantedDevices.length === 0) {
            return false;
          }

          // Connect to the device that was saved to localStorage otherwise use the first one
          const savedPortInfo = JSON.parse(localStorage.getItem('webUSBSerialPort'));
          if (savedPortInfo) {
            for (const device of grantedDevices) {
              if (device.device.vendorId === savedPortInfo.vendorId && device.device.productId === savedPortInfo.productId) {
                this.currentPort = device;
                break;
              }
            }
          }
          if (!this.currentPort) {
            this.currentPort = grantedDevices[0];
          }

          this.setStatus('Connecting to first device...', 'info');
        } else {
          // Prompt the user to select a device
          this.setStatus('Requesting device...', 'info');
          this.currentPort = await serial.requestWebUsbSerialPort();
          first_time_connection = true;
        }

        this.currentPort.onReceiveError = error => this.onReceiveError(error);
        this.currentPort.onReceive = dataView => this.onReceive(dataView);

        try {
          await this.currentPort.connect();

          // save the port to localStorage
          const portInfo = {
            vendorId: this.currentPort.device.vendorId,
            productId: this.currentPort.device.productId,
          }
          localStorage.setItem('webUSBSerialPort', JSON.stringify(portInfo));

          this.setStatus('Connected', 'info');
        } catch (error) {
          if (first_time_connection) {
            // Forget the device if a first time connection fails
            await this.currentPort.forgetDevice();
            this.currentPort = null;
          }
          throw error;
        }

        this.updateUIConnectionState();
      } catch (error) {
        this.setStatus(`Connection failed: ${error.message}`, 'error');
      }
    }

    async reconnectPort() {
      if (this.currentPort) {
        this.setStatus('Reconnecting...', 'info');
        try {
          await this.currentPort.connect();
          this.setStatus('Reconnected', 'info');
        } catch (error) {
          this.setStatus(`Reconnect failed: ${error.message}`, 'error');
        }
      }
      this.updateUIConnectionState();
    }

    async forgetPort() {
      this.stopAutoReconnect();
      if (this.currentPort) {
        await this.currentPort.forgetDevice();
        this.currentPort = null;
        this.setStatus('Device forgotten', 'info');
      } else {
        this.setStatus('No device to forget', 'error');
      }
      this.updateUIConnectionState();
    }

    async forgetAllPorts() {
      this.stopAutoReconnect();
      await this.forgetPort();
      if (serial.isWebUsbSupported()) {
        let ports = await serial.getWebUsbSerialPorts();
        for (const p of ports) {
          await p.forgetDevice();
        }
      }
      this.updateUIConnectionState();
    }

    setNewlineMode() {
      localStorage.setItem('newlineMode', newlineModeSelect.value);
    }

    autoReconnectChanged() {
      if (autoReconnectCheckbox.checked) {
        this.setStatus('Auto-reconnect enabled', 'info');
        this.tryAutoReconnect();
      } else {
        this.setStatus('Auto-reconnect disabled', 'info');
        this.stopAutoReconnect();
      }
      localStorage.setItem('autoReconnect', autoReconnectCheckbox.checked);
    }

    stopAutoReconnect() {
      if (this.reconnectTimeoutId !== null) {
        clearTimeout(this.reconnectTimeoutId);
        this.reconnectTimeoutId = null;
        this.setStatus('Auto-reconnect stopped.', 'info');
      }
    }

    tryAutoReconnect() {
      this.updateUIConnectionState();
      if (!autoReconnectCheckbox.checked) return;
      if (this.reconnectTimeoutId !== null) return; // already trying
      this.setStatus('Attempting to auto-reconnect...', 'info');
      this.reconnectTimeoutId = setTimeout(async () => {
        this.reconnectTimeoutId = null;
        if (!autoReconnectCheckbox.checked) {
          this.setStatus('Auto-reconnect stopped.', 'info');
          return;
        }
        if (this.currentPort) {
          try {
            await this.currentPort.connect();
          } finally {
            this.updateUIConnectionState();
          }
        }
      }, 1000);
    }

    async handleCommandLineInput(e) {
      // Instant mode: send key immediately including special keys like Backspace, arrows, enter, etc.
      if (this.sendMode === 'instant') {
        e.preventDefault();

        // Ignore only pure modifier keys without text representation
        if (e.key.length === 1 ||
          e.key === 'Enter' ||
          e.key === 'Backspace' ||
          e.key === 'Tab' ||
          e.key === 'Escape' ||
          e.key === 'Delete' ) {

          let sendText = '';
          switch (e.key) {
            case 'Enter':
              switch (newlineModeSelect.value) {
                case 'CR': sendText = '\r'; break;
                case 'CRLF': sendText = '\r\n'; break;
                default: sendText = '\n'; break;
              }
              break;
            case 'Backspace':
              // Usually no straightforward char to send for Backspace,
              // but often ASCII DEL '\x7F' or '\b' (0x08) is sent.
              sendText = '\x08'; // backspace
              break;
            case 'Tab':
              sendText = '\t';
              break;
            case 'Escape':
              // Ignore or send ESC control char if needed
              sendText = '\x1B';
              break;
            case 'Delete':
              sendText = '\x7F'; // DEL char
              break;
            default:
              sendText = e.key;
          }
          try {
            await port.send(this.textEncoder.encode(sendText));
          } catch (error) {
            this.setStatus(`Send error: ${error.message}`, 'error');
            this.tryAutoReconnect();
          }
        }

        return;
      }

      // Command mode: handle up/down arrow keys for history
      if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
        e.preventDefault();
        if (this.commandHistory.length === 0) return;
        if (e.key === 'ArrowUp') {
          if (this.commandHistoryIndex === -1) this.commandHistoryIndex = this.commandHistory.length - 1;
          else if (this.commandHistoryIndex > 0) this.commandHistoryIndex--;
        } else if (e.key === 'ArrowDown') {
          if (this.commandHistoryIndex !== -1) this.commandHistoryIndex++;
          if (this.commandHistoryIndex >= this.commandHistory.length) this.commandHistoryIndex = -1;
        }
        commandLineInput.value = this.commandHistoryIndex === -1 ? '' : this.commandHistory[this.commandHistoryIndex];
        return;
      }

      if (e.key !== 'Enter' || !this.currentPort.isConnected) return;
      e.preventDefault();
      const text = commandLineInput.value;
      if (!text) return;

      // Convert to Uint8Array with newline based on config
      let sendText = text;
      switch (newlineModeSelect.value) {
        case 'CR':
          sendText += '\r';
          break;
        case 'CRLF':
          sendText += '\r\n';
          break;
        case 'ANY':
          sendText += '\n';
          break;
      }
      const data = this.textEncoder.encode(sendText);

      try {
        await this.currentPort.send(data);
        this.commandHistoryIndex = -1;
        this.appendCommandToHistory(sendText.replace(/[\r\n]+$/, ''));
        commandLineInput.value = '';
      } catch (error) {
        this.setStatus(`Send error: ${error.message}`, 'error');
        this.tryAutoReconnect();
      }
    }

    toggleSendMode() {
      if (this.sendMode === 'instant') {
        this.setSendMode('command');
      } else {
        this.setSendMode('instant');
      }
    }

    setSendMode(mode) {
      this.sendMode = mode;
      if (mode === 'instant') {
        sendModeBtn.classList.remove('send-mode-command');
        sendModeBtn.classList.add('send-mode-instant');
        sendModeBtn.textContent = 'Instant mode';
      } else {
        sendModeBtn.classList.remove('send-mode-instant');
        sendModeBtn.classList.add('send-mode-command');
        sendModeBtn.textContent = 'Command mode';
      }
      localStorage.setItem('sendMode', this.sendMode);
    }

    normalizeNewlines(text) {
      switch (newlineModeSelect.value) {
        case 'CR':
          return text.replace(/\r?\n/g, '\r');
        case 'CRLF':
          return text.replace(/\r\n|[\r\n]/g, '\r\n');
        case 'ANY':
          return text.replace(/\r\n|\r/g, '\n');
        default:
          return text;
      }
    }

    copyOutput() {
      const text = receivedDataScrollbox.innerText;
      if (text) {
        navigator.clipboard.writeText(text).then(() => {
          this.setStatus('Output copied to clipboard', 'info');
        }, () => {
          this.setStatus('Failed to copy output', 'error');
        });
      } else {
        this.setStatus('No output to copy', 'error');
      }
    }

    resetOutput() {
      receivedDataScrollbox.innerHTML = '';
    }

    async resetAll() {
      await this.forgetAllPorts();

      // Clear localStorage
      for (const key in localStorage) {
        localStorage.removeItem(key);
      }

      // reload the page
      window.location.reload();
    }
  }

  const app = new Application();
})()
