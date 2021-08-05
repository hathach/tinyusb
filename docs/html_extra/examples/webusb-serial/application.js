(function() {
  'use strict';

  document.addEventListener('DOMContentLoaded', event => {
    let connectButton = document.querySelector("#connect");
    let statusDisplay = document.querySelector('#status');
    let port;

    function addLine(linesId, text) {
      var senderLine = document.createElement("div");
      senderLine.className = 'line';
      var textnode = document.createTextNode(text);
      senderLine.appendChild(textnode);
      document.getElementById(linesId).appendChild(senderLine);
      return senderLine;
    }

    let currentReceiverLine;

    function appendLine(linesId, text) {
      if (currentReceiverLine) {
        currentReceiverLine.innerHTML =  currentReceiverLine.innerHTML + text;
      } else {
        currentReceiverLine = addLine(linesId, text);
      }
    }

    function connect() {
      port.connect().then(() => {
        statusDisplay.textContent = '';
        connectButton.textContent = 'Disconnect';

        port.onReceive = data => {
          let textDecoder = new TextDecoder();
          console.log(textDecoder.decode(data));
          if (data.getInt8() === 13) {
            currentReceiverLine = null;
          } else {
            appendLine('receiver_lines', textDecoder.decode(data));
          }
        };
        port.onReceiveError = error => {
          console.error(error);
        };
      }, error => {
        statusDisplay.textContent = error;
      });
    }

    connectButton.addEventListener('click', function() {
      if (port) {
        port.disconnect();
        connectButton.textContent = 'Connect';
        statusDisplay.textContent = '';
        port = null;
      } else {
        serial.requestPort().then(selectedPort => {
          port = selectedPort;
          connect();
        }).catch(error => {
          statusDisplay.textContent = error;
        });
      }
    });

    serial.getPorts().then(ports => {
      if (ports.length === 0) {
        statusDisplay.textContent = 'No device found.';
      } else {
        statusDisplay.textContent = 'Connecting...';
        port = ports[0];
        connect();
      }
    });


    let commandLine = document.getElementById("command_line");

    commandLine.addEventListener("keypress", function(event) {
      if (event.keyCode === 13) {
        if (commandLine.value.length > 0) {
          addLine('sender_lines', commandLine.value);
          commandLine.value = '';
        }
      }

      port.send(new TextEncoder('utf-8').encode(String.fromCharCode(event.which || event.keyCode)));
    });
  });
})();
