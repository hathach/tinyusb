'use strict';

(async () => {
  // bind to the html
  const uiBody = document.body;
  const uiToggleThemeBtn = document.getElementById('theme-toggle');

  const uiConnectWebUsbSerialBtn = document.getElementById('connect_webusb_serial_btn');
  const uiConnectSerialBtn = document.getElementById('connect_serial_btn');
  const uiDisconnectBtn = document.getElementById('disconnect_btn');

  const uiNewlineModeSelect = document.getElementById('newline_mode_select');
  const uiAutoReconnectCheckbox = document.getElementById('auto_reconnect_checkbox');
  const uiForgetDeviceBtn = document.getElementById('forget_device_btn');
  const uiForgetAllDevicesBtn = document.getElementById('forget_all_devices_btn');
  const uiResetAllBtn = document.getElementById('reset_all_btn');
  const uiCopyOutputBtn = document.getElementById('copy_output_btn');
  const uiDownloadOutputCsvBtn = document.getElementById('download_csv_output_btn');

  const uiStatusSpan = document.getElementById('status_span');

  const uiCommandHistoryClearBtn = document.getElementById('clear_command_history_btn');
  const uiCommandHistoryScrollbox = document.getElementById('command_history_scrollbox');
  const uiCommandLineInput = document.getElementById('command_line_input');
  const uiSendModeBtn = document.getElementById('send_mode_btn');

  const uiReceivedDataClearBtn = document.getElementById('clear_received_data_btn');
  const uiReceivedDataScrollbox = document.getElementById('received_data_scrollbox');

  const uiNearTheBottomThreshold = 100; // pixels from the bottom to trigger scroll

  const maxCommandHistoryLength = 123; // max number of command history entries
  const maxReceivedDataLength = 8192 / 8; // max number of received data entries

  const THEME_STATES = ['auto', 'light', 'dark'];

  /// https://stackoverflow.com/a/6234804/4479969
  const escapeHtml = unsafe => {
    if (typeof unsafe !== 'string') unsafe = String(unsafe);
    return unsafe
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;")
      .replaceAll("'", "&#039;");
  };

  class CommandHistoryEntry {
    constructor(text) {
      this.text = text;
      this.time = Date.now();
      this.count = 1;
    }
  }

  class ReceivedDataEntry {
    constructor(text) {
      this.text = text;
      this.time = Date.now();
      this.terminated = false;
    }
  }

  class Application {
    constructor() {
      this.currentPort = null;
      this.textEncoder = new TextEncoder();
      this.textDecoder = new TextDecoder();

      this.reconnectTimeoutId = null;

      this.commandHistory = [];
      this.uiCommandHistoryIndex = -1;

      this.receivedData = [];

      // bind the UI elements
      uiToggleThemeBtn.addEventListener('click', () => this.toggleTheme());
      // Listener for OS Theme Changes
      window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', (e) => {
        const currentPreference = localStorage.getItem('theme') || 'auto';
        // Only act if the user is in automatic mode
        if (currentPreference === 'auto') {
          this.setTheme('auto');
        }
      });

      uiConnectWebUsbSerialBtn.addEventListener('click', () => this.connectWebUsbSerialPort());
      uiConnectSerialBtn.addEventListener('click', () => this.connectSerialPort());
      uiDisconnectBtn.addEventListener('click', () => this.disconnectPort());
      uiNewlineModeSelect.addEventListener('change', () => this.setNewlineMode());
      uiAutoReconnectCheckbox.addEventListener('change', () => this.autoReconnectChanged());
      uiForgetDeviceBtn.addEventListener('click', () => this.forgetPort());
      uiForgetAllDevicesBtn.addEventListener('click', () => this.forgetAllPorts());
      uiResetAllBtn.addEventListener('click', () => this.resetAll());
      uiCopyOutputBtn.addEventListener('click', () => this.copyOutput());
      uiDownloadOutputCsvBtn.addEventListener('click', () => this.downloadOutputCsv());
      uiCommandHistoryClearBtn.addEventListener('click', () => this.clearCommandHistory());
      uiCommandLineInput.addEventListener('keydown', (e) => this.handleCommandLineInput(e));
      uiSendModeBtn.addEventListener('click', () => this.toggleSendMode());
      uiReceivedDataClearBtn.addEventListener('click', () => this.clearReceivedData());

      window.addEventListener('beforeunload', () => this.beforeUnloadHandler());

      // restore state from localStorage
      try {
        this.restoreState();
      } catch (error) {
        console.error('Failed to restore state from localStorage', error);
        this.resetAll();
        this.restoreState();
      }

      this.updateUIConnectionState();
      this.connectWebUsbSerialPort(true);
    }

    beforeUnloadHandler() {
      // Save the scroll position of the command history and received data
      localStorage.setItem('commandHistoryScrollTop', uiCommandHistoryScrollbox.scrollTop);
      localStorage.setItem('receivedDataScrollTop', uiReceivedDataScrollbox.scrollTop);
    }

    restoreState() {
      // Restore theme choice
      const savedTheme = localStorage.getItem('theme');
      if (savedTheme) {
        this.setTheme(savedTheme);
      }

      // Restore command history
      let savedCommandHistory = JSON.parse(localStorage.getItem('commandHistory') || '[]');
      for (const cmd of savedCommandHistory) {
        this.addCommandToHistoryUI(cmd);
      }
      // Restore scroll position for command history
      const commandHistoryScrollTop = localStorage.getItem('commandHistoryScrollTop');
      if (commandHistoryScrollTop) {
        uiCommandHistoryScrollbox.scrollTop = parseInt(commandHistoryScrollTop, 10);
      }

      // Restore received data
      let savedReceivedData = JSON.parse(localStorage.getItem('receivedData') || '[]');
      for (let line of savedReceivedData) {
        line.terminated = true;
        this.addReceivedDataEntryUI(line);
      }
      // Restore scroll position for received data
      const receivedDataScrollTop = localStorage.getItem('receivedDataScrollTop');
      if (receivedDataScrollTop) {
        uiReceivedDataScrollbox.scrollTop = parseInt(receivedDataScrollTop, 10);
      }

      this.sendMode = localStorage.getItem('sendMode') || 'command';
      this.setSendMode(this.sendMode);

      uiAutoReconnectCheckbox.checked = !(localStorage.getItem('autoReconnect') === 'false');

      let savedNewlineMode = localStorage.getItem('newlineMode');
      if (savedNewlineMode) {
        uiNewlineModeSelect.value = savedNewlineMode;
      }
    }

    setTheme(theme) {
      const modeName = theme.charAt(0).toUpperCase() + theme.slice(1);
      uiToggleThemeBtn.textContent = `Theme: ${modeName}`;

      if (theme === 'auto') {
        // In auto mode, we rely on the OS preference.
        // We check the media query and add/remove the class accordingly.
        const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
        if (prefersDark) {
          uiBody.classList.add('dark-mode');
        } else {
          uiBody.classList.remove('dark-mode');
        }
      } else if (theme === 'light') {
        // Force light mode by removing the class.
        uiBody.classList.remove('dark-mode');
      } else if (theme === 'dark') {
        // Force dark mode by adding the class.
        uiBody.classList.add('dark-mode');
      }

      // Save the theme to localStorage
      localStorage.setItem('theme', theme);
    }

    toggleTheme() {
      const currentTheme = localStorage.getItem('theme') || 'auto';
      const nextThemeIndex = (THEME_STATES.indexOf(currentTheme) + 1) % THEME_STATES.length;
      const nextTheme = THEME_STATES[nextThemeIndex];
      this.setTheme(nextTheme);
    }

    addCommandToHistoryUI(commandHistoryEntry) {
      let commandHistoryEntryBtn = null;

      let lastCommandMatched = false;
      if (this.commandHistory.length > 0) {
        let lastCommandEntry = this.commandHistory[this.commandHistory.length - 1];
        if (lastCommandEntry.text === commandHistoryEntry.text) {
          lastCommandEntry.count++;
          lastCommandEntry.time = Date.now();
          lastCommandMatched = true;

          // Update the last command entry
          commandHistoryEntryBtn = uiCommandHistoryScrollbox.lastElementChild;
          let time_str = new Date(lastCommandEntry.time).toLocaleString();
          commandHistoryEntryBtn.querySelector('.command-history-entry-time').textContent = time_str;
          commandHistoryEntryBtn.querySelector('.command-history-entry-text').textContent = lastCommandEntry.text;
          commandHistoryEntryBtn.querySelector('.command-history-entry-count').textContent = '×' + lastCommandEntry.count;
        }
      }
      if (!lastCommandMatched) {
        this.commandHistory.push(commandHistoryEntry);

        // Create a new command history entry
        commandHistoryEntryBtn = document.createElement('button');
        commandHistoryEntryBtn.className = 'command-history-entry';
        commandHistoryEntryBtn.type = 'button';
        let time_str = new Date(commandHistoryEntry.time).toLocaleString();
        commandHistoryEntryBtn.innerHTML = `
          <span class="command-history-entry-time">${escapeHtml(time_str)}</span>
          <span class="command-history-entry-text">${escapeHtml(commandHistoryEntry.text)}</span>
          <span class="command-history-entry-count">×${escapeHtml(commandHistoryEntry.count)}</span>
        `;
        commandHistoryEntryBtn.addEventListener('click', () => {
          if (uiCommandLineInput.disabled) return;
          uiCommandLineInput.value = commandHistoryEntry.text;
          uiCommandLineInput.focus();
        });

        uiCommandHistoryScrollbox.appendChild(commandHistoryEntryBtn);
      }

      // Limit the command history length
      while (this.commandHistory.length > maxCommandHistoryLength) {
        this.commandHistory.shift();
        uiCommandHistoryScrollbox.removeChild(uiCommandHistoryScrollbox.firstElementChild);
      }
    }

    appendNewCommandToHistory(commandHistoryEntry) {
      const wasNearBottom = this.isNearBottom(uiCommandHistoryScrollbox);

      this.addCommandToHistoryUI(commandHistoryEntry);

      // Save the command history to localStorage
      localStorage.setItem('commandHistory', JSON.stringify(this.commandHistory));

      // Scroll to the new entry if near the bottom
      if (wasNearBottom) {
        this.scrollToBottom(uiCommandHistoryScrollbox);
      }
    }

    clearCommandHistory() {
      this.commandHistory = [];
      uiCommandHistoryScrollbox.textContent = '';
      localStorage.removeItem('commandHistory');
      this.setStatus('Command history cleared', 'info');
    }

    isNearBottom(container) {
      return container.scrollHeight - container.scrollTop <= container.clientHeight + uiNearTheBottomThreshold;
    }

    scrollToBottom(container) {
      requestAnimationFrame(() => {
        container.scrollTop = container.scrollHeight;
      });
    }

    addReceivedDataEntryUI(receivedDataEntry) {
      let newReceivedDataEntries = [];
      let updateLastReceivedDataEntry = false;
      if (this.receivedData.length <= 0) {
        newReceivedDataEntries.push(receivedDataEntry);
      } else {
        let lastReceivedDataEntry = this.receivedData[this.receivedData.length - 1];
        // Check if the last entry is terminated
        if (lastReceivedDataEntry.terminated) {
          newReceivedDataEntries.push(receivedDataEntry);
        } else {
          if (!lastReceivedDataEntry.terminated) {
            updateLastReceivedDataEntry = true;
            this.receivedData.pop();
            receivedDataEntry.text = lastReceivedDataEntry.text + receivedDataEntry.text;
          }
          // split the text into lines
          let lines = receivedDataEntry.text.split(/\r?\n/);
          // check if the last line is terminated by checking if it ends with an empty string
          let lastLineTerminated = lines[lines.length - 1] === '';
          if (lastLineTerminated) {
            lines.pop(); // remove the last empty line
          }

          // create new entries for each line
          for (let i = 0; i < lines.length; i++) {
            let line = lines[i];
            let entry = new ReceivedDataEntry(line);
            if (i === lines.length - 1) {
              entry.terminated = lastLineTerminated;
            } else {
              entry.terminated = true;
            }
            newReceivedDataEntries.push(entry);
          }
          // if the last line is terminated, modify the last entry
          if (lastLineTerminated) {
            newReceivedDataEntries[newReceivedDataEntries.length - 1].terminated = true;
          } else {
            newReceivedDataEntries[newReceivedDataEntries.length - 1].terminated = false;
          }
        }
      }

      this.receivedData.push(...newReceivedDataEntries);

      if (updateLastReceivedDataEntry) {
        // update the rendering of the last entry
        let lastReceivedDataEntryBtn = uiReceivedDataScrollbox.lastElementChild;
        lastReceivedDataEntryBtn.querySelector('.received-data-entry-text').textContent = newReceivedDataEntries[0].text;
        lastReceivedDataEntryBtn.querySelector('.received-data-entry-time').textContent = new Date(newReceivedDataEntries[0].time).toLocaleString();
        newReceivedDataEntries.shift();
      }

      // render the new entries
      let documentFragment = document.createDocumentFragment();
      for (const entry of newReceivedDataEntries) {
        let receivedDataEntryBtn = document.createElement('div');
        receivedDataEntryBtn.className = 'received-data-entry';
        receivedDataEntryBtn.innerHTML = `
          <span class="received-data-entry-time">${escapeHtml(new Date(entry.time).toLocaleString())}</span>
          <span class="received-data-entry-text">${escapeHtml(entry.text)}</span>
        `;
        documentFragment.appendChild(receivedDataEntryBtn);
      }
      uiReceivedDataScrollbox.appendChild(documentFragment);

      // Limit the received data length
      while (this.receivedData.length > maxReceivedDataLength) {
        this.receivedData.shift();
        uiReceivedDataScrollbox.removeChild(uiReceivedDataScrollbox.firstElementChild);
      }
    }

    appendNewReceivedData(receivedDataEntry) {
      const wasNearBottom = this.isNearBottom(uiReceivedDataScrollbox);

      this.addReceivedDataEntryUI(receivedDataEntry);

      // Save the received data to localStorage
      localStorage.setItem('receivedData', JSON.stringify(this.receivedData));

      // Scroll to the new entry if near the bottom
      if (wasNearBottom) {
        this.scrollToBottom(uiReceivedDataScrollbox);
      }
    }

    clearReceivedData() {
      this.receivedData = [];
      uiReceivedDataScrollbox.textContent = '';
      localStorage.removeItem('receivedData');
      this.setStatus('Received data cleared', 'info');
    }

    setStatus(msg, level = 'info') {
      console.error(msg);
      uiStatusSpan.textContent = msg;
      uiStatusSpan.className = 'status status-' + level;
    }

    /// force_connected is used to instantly change the UI to the connected state while the device is still connecting
    /// Otherwise we would have to wait for the connection to be established.
    /// This can take until the device sends the first data packet.
    updateUIConnectionState(force_connected = false) {
      if (force_connected || (this.currentPort && this.currentPort.isConnected)) {
        uiConnectWebUsbSerialBtn.style.display = 'none';
        uiConnectSerialBtn.style.display = 'none';
        uiDisconnectBtn.style.display = 'block';
        uiCommandLineInput.disabled = false;

        if (this.currentPort instanceof SerialPort) {
          uiDisconnectBtn.textContent = 'Disconnect from WebSerial';
        } else if (this.currentPort instanceof WebUsbSerialPort) {
          uiDisconnectBtn.textContent = 'Disconnect from WebUSB';
        } else {
          uiDisconnectBtn.textContent = 'Disconnect';
        }
      } else {
        if (serial.isWebUsbSupported()) {
          uiConnectWebUsbSerialBtn.style.display = 'block';
        }
        if (serial.isWebSerialSupported()) {
          uiConnectSerialBtn.style.display = 'block';
        }
        if (!serial.isWebUsbSupported() && !serial.isWebSerialSupported()) {
          this.setStatus('Your browser does not support WebUSB or WebSerial', 'error');
        }
        uiDisconnectBtn.style.display = 'none';
        uiCommandLineInput.disabled = true;
        uiCommandLineInput.value = '';
        uiCommandLineInput.blur();
      }
    }

    async disconnectPort() {
      this.stopAutoReconnect();

      if (!this.currentPort) {
        this.updateUIConnectionState();
        return;
      };

      try {
        await this.currentPort.disconnect();
        this.setStatus('Disconnected', 'info');
      }
      catch (error) {
        this.setStatus(`Disconnect error: ${error.message}`, 'error');
      }

      this.updateUIConnectionState();
    }

    async onReceive(dataView) {
      this.updateUIConnectionState();

      let text = this.textDecoder.decode(dataView);
      let receivedDataEntry = new ReceivedDataEntry(text);
      this.appendNewReceivedData(receivedDataEntry);
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
        this.updateUIConnectionState(true);
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
      } finally {
        this.updateUIConnectionState();
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
          if (!uiAutoReconnectCheckbox.checked || grantedDevices.length === 0) {
            return false;
          }

          // Connect to the device that was saved to localStorage otherwise use the first one
          const savedPortInfo = JSON.parse(localStorage.getItem('webUSBSerialPort'));
          if (savedPortInfo) {
            for (const device of grantedDevices) {
              if (device._device.vendorId === savedPortInfo.vendorId && device._device.productId === savedPortInfo.productId) {
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
          this.updateUIConnectionState(true);
          await this.currentPort.connect();

          // save the port to localStorage
          const portInfo = {
            vendorId: this.currentPort._device.vendorId,
            productId: this.currentPort._device.productId,
          }
          localStorage.setItem('webUSBSerialPort', JSON.stringify(portInfo));

          this.setStatus('Connected', 'info');
          uiCommandLineInput.focus();
        } catch (error) {
          if (first_time_connection) {
            // Forget the device if a first time connection fails
            await this.currentPort.forgetDevice();
            this.currentPort = null;
          }
          throw error;
        } finally {
          this.updateUIConnectionState();
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
        } finally {
          this.updateUIConnectionState();
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
      localStorage.setItem('newlineMode', uiNewlineModeSelect.value);
    }

    autoReconnectChanged() {
      if (uiAutoReconnectCheckbox.checked) {
        this.setStatus('Auto-reconnect enabled', 'info');
        this.tryAutoReconnect();
      } else {
        this.setStatus('Auto-reconnect disabled', 'info');
        this.stopAutoReconnect();
      }
      localStorage.setItem('autoReconnect', uiAutoReconnectCheckbox.checked);
    }

    stopAutoReconnect() {
      if (this.reconnectTimeoutId !== null) {
        clearTimeout(this.reconnectTimeoutId);
        this.reconnectTimeoutId = null;
        this.setStatus('Auto-reconnect stopped.', 'info');
      }
    }

    async autoReconnectTimeout() {
        this.reconnectTimeoutId = null;
        if (!uiAutoReconnectCheckbox.checked) {
          this.setStatus('Auto-reconnect stopped.', 'info');
          return;
        }
        if (this.currentPort && !this.currentPort.isConnected) {
          try {
            await this.currentPort.connect();
            this.setStatus('Reconnected successfully', 'info');
          } catch (error) {
            this.setStatus(`Reconnect failed: ${error.message}`, 'error');
            // Try again after a delay
            this.tryAutoReconnect();
          } finally {
            this.updateUIConnectionState();
          }
        }
    }

    tryAutoReconnect() {
      this.updateUIConnectionState();
      if (!uiAutoReconnectCheckbox.checked) return;
      if (this.reconnectTimeoutId !== null) return; // already trying
      this.setStatus('Attempting to auto-reconnect...', 'info');
      this.reconnectTimeoutId = setTimeout(async () => {
        await this.autoReconnectTimeout();
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
              switch (uiNewlineModeSelect.value) {
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
            await this.currentPort.send(this.textEncoder.encode(sendText));
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
          if (this.uiCommandHistoryIndex === -1) this.uiCommandHistoryIndex = this.commandHistory.length - 1;
          else if (this.uiCommandHistoryIndex > 0) this.uiCommandHistoryIndex--;
        } else if (e.key === 'ArrowDown') {
          if (this.uiCommandHistoryIndex !== -1) this.uiCommandHistoryIndex++;
          if (this.uiCommandHistoryIndex >= this.commandHistory.length) this.uiCommandHistoryIndex = -1;
        }
        uiCommandLineInput.value = this.uiCommandHistoryIndex === -1 ? '' : this.commandHistory[this.uiCommandHistoryIndex].text;
        return;
      }

      if (e.key !== 'Enter' || !this.currentPort.isConnected) return;
      e.preventDefault();
      const text = uiCommandLineInput.value;
      if (!text) return;

      // Convert to Uint8Array with newline based on config
      let sendText = text;
      switch (uiNewlineModeSelect.value) {
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
        this.uiCommandHistoryIndex = -1;
        let history_cmd_text = sendText.replace(/[\r\n]+$/, '');
        let history_entry = new CommandHistoryEntry(history_cmd_text);
        this.appendNewCommandToHistory(history_entry);
        uiCommandLineInput.value = '';
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
        uiSendModeBtn.classList.remove('send-mode-command');
        uiSendModeBtn.classList.add('send-mode-instant');
        uiSendModeBtn.textContent = 'Instant mode';
      } else {
        uiSendModeBtn.classList.remove('send-mode-instant');
        uiSendModeBtn.classList.add('send-mode-command');
        uiSendModeBtn.textContent = 'Command mode';
      }
      localStorage.setItem('sendMode', this.sendMode);
    }

    copyOutput() {
      let text = '';
      for (const entry of this.receivedData) {
        text += entry.text;
        if (entry.terminated) {
          text += '\n';
        }
      }

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

    downloadOutputCsv() {
      // save <iso_date_time>,<received_line>
      let csvContent = 'data:text/csv;charset=utf-8,';
      for (const entry of this.receivedData) {
        let sanitizedText = entry.text.replace(/"/g, '""').replace(/[\r\n]+$/, '');
        let line = new Date(entry.time).toISOString() + ',"' + sanitizedText + '"';
        csvContent += line + '\n';
      }

      const encodedUri = encodeURI(csvContent);
      const link = document.createElement('a');
      link.setAttribute('href', encodedUri);
      const filename = new Date().toISOString().replace(/:/g, '-') + '_tinyusb_received_serial_data.csv';
      link.setAttribute('download', filename);
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
    }

    async resetAll() {
      await this.forgetAllPorts();

      // Clear localStorage
      localStorage.clear();

      // reload the page
      window.location.reload();
    }
  }

  const app = new Application();
})()
