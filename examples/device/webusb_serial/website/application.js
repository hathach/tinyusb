'use strict';

(() => {
  const connectBtn = document.getElementById('connect');
  const resetAllBtn = document.getElementById('reset_all');
  const resetOutputBtn = document.getElementById('reset_output');
  const senderLines = document.getElementById('sender_lines');
  const receiverLines = document.getElementById('receiver_lines');
  const commandLine = document.getElementById('command_line');
  const status = document.getElementById('status');
  const newlineModeSelect = document.getElementById('newline_mode');
  const sendModeBtn = document.getElementById('send_mode');
  const autoReconnectCheckbox = document.getElementById('auto_reconnect');
  const forgetDeviceBtn = document.getElementById('forget_device');
  const forgetAllDevicesBtn = document.getElementById('forget_all_devices');

  const nearTheBottomThreshold = 100; // pixels from the bottom to trigger scroll

  let port = null;
  let lastPort = null; // for reconnecting and initial connection

  const history = [];
  let historyIndex = -1;
  let lastCommand = null;
  let lastCommandCount = 0;
  let lastCommandButton = null;

  let sendMode = 'command'; // default mode

  let reconnectIntervalId = null; // track reconnect interval

  // Format incoming data to string based on newline mode
  const decodeData = (() => {
    const decoder = new TextDecoder();
    return dataView => decoder.decode(dataView);
  })();

  // Normalize newline if mode is ANY
  const normalizeNewlines = (text, mode) => {
    switch (mode) {
      case 'CR':
        // Only \r: Replace all \n with \r
        return text.replace(/\r?\n/g, '\r');
      case 'CRLF':
        // Replace lone \r or \n with \r\n
        return text.replace(/\r\n|[\r\n]/g, '\r\n');
      case 'ANY':
        // Accept any \r, \n, \r\n. Normalize as \n for display
        return text.replace(/\r\n|\r/g, '\n');
      default:
        return text;
    }
  };

  // Append line to container, optionally scroll to bottom
  const appendLineToReceiver = (container, text, className = '') => {
    const div = document.createElement('div');
    if (className) div.className = className;
    div.textContent = text;
    container.appendChild(div);

    const distanceFromBottom = container.scrollHeight - (container.scrollTop + container.clientHeight);
    if (distanceFromBottom < nearTheBottomThreshold) {
      requestAnimationFrame(() => {
        div.scrollIntoView({ behavior: "instant" });
      });
    }
  };

  // Append sent command to sender container as a clickable element
  const appendCommandToSender = (container, text) => {
    if (text === lastCommand) {
      // Increment count and update button
      lastCommandCount++;
      lastCommandButton.textContent = `${text} ×${lastCommandCount}`;
    } else {
      // Reset count and add new button
      lastCommand = text;
      lastCommandCount = 1;

      const commandEl = document.createElement('button');
      commandEl.className = 'sender-entry';
      commandEl.type = 'button';
      commandEl.textContent = text;
      commandEl.addEventListener('click', () => {
        commandLine.value = text;
        commandLine.focus();
      });
      container.appendChild(commandEl);
      lastCommandButton = commandEl;

      const distanceFromBottom = container.scrollHeight - (container.scrollTop + container.clientHeight);
      if (distanceFromBottom < nearTheBottomThreshold) {
        requestAnimationFrame(() => {
          commandEl.scrollIntoView({ behavior: 'instant' });
        });
      }
    }
  };

  // Update status text and style
  const setStatus = (msg, level = 'info') => {
    console.log(msg);
    status.textContent = msg;
    status.className = 'status status-' + level;
  };

  // Disconnect helper
  const disconnectPort = async () => {
    if (port) {
      try {
        await port.disconnect();
      } catch (error) {
        setStatus(`Disconnect error: ${error.message}`, 'error');
      }
      port = null;
      connectBtn.textContent = 'Connect';
      commandLine.disabled = true;
    }
  };

  // Connect helper
  const connectPort = async (initial=false) => {
    try {
      let grantedDevices = await serial.getPorts();
      if (grantedDevices.length === 0 && initial) {
        return false;
      }
      if (grantedDevices.length === 0) {
        // No previously granted devices, request a new one
        setStatus('Requesting device...', 'info');
        port = await serial.requestPort();
      } else {
        if (lastPort) {
          // Try to reconnect to the last used port
          const matchingPort = grantedDevices.find(p => p.portPointToSameDevice(lastPort));
          if (matchingPort) {
            port = matchingPort;
            setStatus('Reconnecting to last device...', 'info');
          } else {
            return false;
          }
        } else {
          // No last port, just use the first available
          port = grantedDevices[0];
          setStatus('Connecting to first device...', 'info');
        }
      }

      await port.connect();
      lastPort = port; // save for reconnecting

      setStatus(`Connected to ${port.device.productName || 'device'}`, 'info');
      connectBtn.textContent = 'Disconnect';
      commandLine.disabled = false;
      commandLine.focus();

      port.onReceive = dataView => {
        let text = decodeData(dataView);
        text = normalizeNewlines(text, newlineModeSelect.value);
        appendLineToReceiver(receiverLines, text, 'received');
      };

      port.onReceiveError = error => {
        setStatus(`Read error: ${error.message}`, 'error');
        // Start auto reconnect on error if enabled
        tryAutoReconnect();
      };
      return true;
    } catch (error) {
      setStatus(`Connection failed: ${error.message}`, 'error');
      port = null;
      connectBtn.textContent = 'Connect';
      commandLine.disabled = true;
      return false;
    }
  };

  // Start auto reconnect interval if checkbox is checked and not already running
  const tryAutoReconnect = () => {
    if (!autoReconnectCheckbox.checked) return;
    if (reconnectIntervalId !== null) return; // already trying
    setStatus('Attempting to auto-reconnect...', 'info');
    reconnectIntervalId = setInterval(async () => {
      if (!autoReconnectCheckbox.checked) {
        clearInterval(reconnectIntervalId);
        reconnectIntervalId = null;
        setStatus('Auto-reconnect stopped.', 'info');
        return;
      }
      await disconnectPort();
      const success = await connectPort();
      if (success) {
        clearInterval(reconnectIntervalId);
        reconnectIntervalId = null;
        setStatus('Reconnected successfully.', 'info');
      }
    }, 1000);
  };

  // Stop auto reconnect immediately
  const stopAutoReconnect = () => {
    if (reconnectIntervalId !== null) {
      clearInterval(reconnectIntervalId);
      reconnectIntervalId = null;
      setStatus('Auto-reconnect stopped.', 'info');
    }
  };

  // Connect button click handler
  connectBtn.addEventListener('click', async () => {
    if (!serial.isWebUsbSupported()) {
      setStatus('WebUSB not supported on this browser', 'error');
      return;
    }

    if (port) {
      // Disconnect
      stopAutoReconnect();
      await disconnectPort();
      setStatus('Disconnected', 'info');
      return;
    }

    stopAutoReconnect();
    try {
      // Connect
      const success = await connectPort();
      if (success) {
        setStatus('Connected', 'info');
      }
    } catch (error) {
      setStatus(`Connection failed: ${error.message}`, 'error');
      port = null;
      connectBtn.textContent = 'Connect';
      commandLine.disabled = true;
    }
  });

  // Checkbox toggle stops auto reconnect if unchecked
  autoReconnectCheckbox.addEventListener('change', () => {
    if (!autoReconnectCheckbox.checked) {
      stopAutoReconnect();
    } else {
      // Start auto reconnect immediately if not connected
      if (!port) {
        tryAutoReconnect();
      }
    }
  });

  sendModeBtn.addEventListener('click', () => {
    if (sendMode === 'command') {
      sendMode = 'instant';
      sendModeBtn.classList.remove('send-mode-command');
      sendModeBtn.classList.add('send-mode-instant');
      sendModeBtn.textContent = 'Instant mode';
      // In instant mode, we clear the command line
      commandLine.value = '';
    } else {
      sendMode = 'command';
      sendModeBtn.classList.remove('send-mode-instant');
      sendModeBtn.classList.add('send-mode-command');
      sendModeBtn.textContent = 'Command mode';
    }
  });

  // Send command line input on Enter
  commandLine.addEventListener('keydown', async e => {
    if (!port) return;

    // Instant mode: send key immediately including special keys like Backspace, arrows, enter, etc.
    if (sendMode === 'instant') {
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

        const encoder = new TextEncoder();
        try {
          await port.send(encoder.encode(sendText));
        } catch (error) {
          setStatus(`Send error: ${error.message}`, 'error');
          tryAutoReconnect();
        }
      }

      return;
    }

    // Command mode: handle up/down arrow keys for history
    if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
      e.preventDefault();
      if (history.length === 0) return;
      if (e.key === 'ArrowUp') {
        if (historyIndex === -1) historyIndex = history.length - 1;
        else if (historyIndex > 0) historyIndex--;
      } else if (e.key === 'ArrowDown') {
        if (historyIndex !== -1) historyIndex++;
        if (historyIndex >= history.length) historyIndex = -1;
      }
      commandLine.value = historyIndex === -1 ? '' : history[historyIndex];
      return;
    }

    if (e.key !== 'Enter' || !port) return;
    e.preventDefault();
    const text = commandLine.value;
    if (!text) return;

    // Add command to history, ignore duplicate consecutive
    if (history.length === 0 || history[history.length - 1] !== text) {
      history.push(text);
    }
    historyIndex = -1;

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
    const encoder = new TextEncoder();
    const data = encoder.encode(sendText);

    try {
      await port.send(data);
      appendCommandToSender(senderLines, sendText.replace(/[\r\n]+$/, ''));
      commandLine.value = '';
    } catch (error) {
      setStatus(`Send error: ${error.message}`, 'error');
      tryAutoReconnect();
    }
  });

  // Forget device button clears stored device info
  forgetDeviceBtn.addEventListener('click', async () => {
    if (port) {
      // Disconnect first
      await port.disconnect();
      await port.forgetDevice();
      stopAutoReconnect();
      await disconnectPort();

      setStatus('Device forgotten', 'info');
    } else {
      setStatus('No device to forget', 'error');
    }
  });

  // Forget all devices button clears all stored device info
  forgetAllDevicesBtn.addEventListener('click', async () => {
    stopAutoReconnect();
    await disconnectPort();
    let ports = await serial.getPorts();
    if (ports.length > 0) {
      for (const p of ports) {
        await p.forgetDevice();
      }
      setStatus('All devices forgotten', 'info');
    } else {
      setStatus('No devices to forget', 'error');
    }
  });

  // Reset output button clears receiver
  resetOutputBtn.addEventListener('click', () => {
    receiverLines.innerHTML = '';
  });

  // Reset button clears sender and receiver
  resetAllBtn.addEventListener('click', () => {
    senderLines.innerHTML = '';
    receiverLines.innerHTML = '';
    lastCommand = null;
    lastCommandCount = 0;
    lastCommandButton = null;
  });


  // Disable input on load
  commandLine.disabled = true;

  // Show warning if no WebUSB support
  if (!serial.isWebUsbSupported()) {
    setStatus('WebUSB not supported on this browser', 'error');
  } else {
    // try to connect to any available device
    connectPort(true);
  }
})();
