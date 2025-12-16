const SERVICE_UUID = "01931c44-3867-7740-9867-c822cb7df308";
const OTA_CHARACTERISTIC_UUID = "01931c44-3867-7427-96ab-8d7ac0ae09ee";
const CHUNK_SIZE = 180; // Conservative payload per BLE packet

const connectBtn = document.getElementById("connect-btn");
const uploadBtn = document.getElementById("upload-btn");
const fileInput = document.getElementById("firmware-file");
const fileLabel = document.getElementById("file-label");
const fileHelper = document.getElementById("file-helper");
const connectionStatus = document.getElementById("connection-status");
const deviceName = document.getElementById("device-name");
const progressBar = document.getElementById("progress-bar");
const progressLabel = document.getElementById("progress-label");
const logEl = document.getElementById("log");

let bluetoothDevice;
let otaCharacteristic;
let firmwareFile;
let isUploading = false;

function log(message, type = "info") {
  const entry = document.createElement("p");
  entry.textContent = message;
  entry.className = type;
  logEl.appendChild(entry);
  logEl.scrollTop = logEl.scrollHeight;
}

function setProgress(percent, label) {
  progressBar.style.width = `${percent}%`;
  progressLabel.textContent = label;
}

function setConnectedState(name) {
  connectionStatus.textContent = "Connected";
  connectionStatus.classList.remove("error");
  connectionStatus.classList.add("success");
  deviceName.textContent = name || "LumiFur Controller";
  uploadBtn.disabled = !firmwareFile;
  connectBtn.textContent = "Reconnect";
}

function setDisconnectedState(reason = "Disconnected") {
  connectionStatus.textContent = reason;
  connectionStatus.classList.remove("success");
  connectionStatus.classList.add("error");
  deviceName.textContent = "—";
  uploadBtn.disabled = true;
  isUploading = false;
  connectBtn.textContent = "Connect";
  otaCharacteristic = undefined;
  bluetoothDevice = undefined;
  setProgress(0, "Waiting to start…");
}

function handleNotifications(event) {
  const value = event.target.value;
  if (!value || value.byteLength < 1) return;

  const view = new DataView(value.buffer);
  const code = view.getUint8(0);

  if (code === 0x01 && value.byteLength >= 2 && view.getUint8(1) === 0x00) {
    log("OTA session started", "success");
  } else if (code === 0x03 && value.byteLength >= 2 && view.getUint8(1) === 0x00) {
    log("OTA complete. Device will reboot.", "success");
    setProgress(100, "Upload complete");
    uploadBtn.disabled = false;
    isUploading = false;
  } else if (code === 0x04 && value.byteLength >= 2 && view.getUint8(1) === 0x00) {
    log("OTA aborted by device", "error");
    isUploading = false;
    uploadBtn.disabled = false;
  } else if (code === 0xff && value.byteLength >= 2) {
    log(`Device reported error 0x${view.getUint8(1).toString(16).padStart(2, "0")}`, "error");
    isUploading = false;
    uploadBtn.disabled = false;
  }
}

async function connect() {
  if (!navigator.bluetooth) {
    log("Web Bluetooth is not available in this browser.", "error");
    return;
  }

  if (!window.isSecureContext) {
    log("A secure context is required. Serve this page via https:// or http://localhost.", "error");
    return;
  }

  if (bluetoothDevice?.gatt?.connected && otaCharacteristic) {
    setConnectedState(bluetoothDevice.name);
    log("Already connected to the device.");
    return;
  }

  try {
    bluetoothDevice = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
      optionalServices: [SERVICE_UUID],
    });

    bluetoothDevice.addEventListener("gattserverdisconnected", () => {
      log("Bluetooth connection lost.", "error");
      setDisconnectedState("Disconnected");
    });

    const server = await bluetoothDevice.gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    otaCharacteristic = await service.getCharacteristic(OTA_CHARACTERISTIC_UUID);
    await otaCharacteristic.startNotifications();
    otaCharacteristic.addEventListener("characteristicvaluechanged", handleNotifications);

    setConnectedState(bluetoothDevice.name);
    log("Connected to device and ready for OTA.", "success");
  } catch (error) {
    if (error.name === "NotFoundError") {
      log("Device selection canceled.");
      return;
    }

    log(`Connection failed: ${error.message}`, "error");
    setDisconnectedState("Failed to connect");
  }
}

function buildStartPacket(size) {
  const buffer = new ArrayBuffer(5);
  const view = new DataView(buffer);
  view.setUint8(0, 0x01);
  view.setUint32(1, size, true);
  return buffer;
}

async function upload() {
  if (!otaCharacteristic || !bluetoothDevice?.gatt.connected) {
    log("Connect to the device before uploading.", "error");
    return;
  }
  if (!firmwareFile) {
    log("Choose a firmware .bin file first.", "error");
    return;
  }
  if (isUploading) return;

  isUploading = true;
  uploadBtn.disabled = true;
  setProgress(0, "Sending start command…");
  log(`Beginning OTA upload (${firmwareFile.size} bytes).`);

  try {
    const firmwareData = new Uint8Array(await firmwareFile.arrayBuffer());

    await otaCharacteristic.writeValueWithResponse(buildStartPacket(firmwareData.byteLength));

    for (let offset = 0; offset < firmwareData.length; offset += CHUNK_SIZE) {
      const remaining = firmwareData.length - offset;
      const chunkLength = Math.min(remaining, CHUNK_SIZE);
      const packet = new Uint8Array(chunkLength + 1);
      packet[0] = 0x02; // DATA
      packet.set(firmwareData.subarray(offset, offset + chunkLength), 1);

      await otaCharacteristic.writeValueWithResponse(packet);

      const percent = Math.min(99, Math.round(((offset + chunkLength) / firmwareData.length) * 100));
      setProgress(percent, `Sending firmware… ${percent}%`);
    }

    setProgress(99, "Finalizing update…");
    await otaCharacteristic.writeValueWithResponse(Uint8Array.of(0x03)); // END
    log("End command sent. Waiting for device to reboot…");
  } catch (error) {
    log(`Upload failed: ${error.message}`, "error");
    isUploading = false;
    uploadBtn.disabled = false;
  }
}

fileInput.addEventListener("change", (event) => {
  const [file] = event.target.files;
  firmwareFile = file;

  if (file) {
    fileLabel.textContent = file.name;
    fileHelper.textContent = `${file.size.toLocaleString()} bytes selected`;
    uploadBtn.disabled = !otaCharacteristic;
  } else {
    fileLabel.textContent = "Choose .bin file";
    fileHelper.textContent = "No file selected";
    uploadBtn.disabled = true;
  }
});

if (!navigator.bluetooth) {
  connectionStatus.textContent = "Unsupported";
  connectionStatus.classList.add("error");
  connectBtn.disabled = true;
  log("This browser does not support Web Bluetooth.", "error");
} else if (!window.isSecureContext) {
  log("Serve this page from https:// or http://localhost to enable Web Bluetooth.", "error");
}

window.addEventListener("beforeunload", () => {
  if (bluetoothDevice?.gatt?.connected) {
    bluetoothDevice.gatt.disconnect();
  }
});

connectBtn.addEventListener("click", connect);
uploadBtn.addEventListener("click", upload);
