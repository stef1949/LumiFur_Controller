const SERVICE_UUID = "01931c44-3867-7740-9867-c822cb7df308";
const OTA_CHARACTERISTIC_UUID = "01931c44-3867-7427-96ab-8d7ac0ae09ee";

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
let activeTransfer;
let uploadCompleted = false;

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
  uploadBtn.disabled = isUploading || !firmwareFile;
  connectBtn.textContent = "Reconnect";
}

function setDisconnectedState(reason = "Disconnected") {
  connectionStatus.textContent = reason;
  connectionStatus.classList.remove("success");
  connectionStatus.classList.add("error");
  deviceName.textContent = "—";
  uploadBtn.disabled = true;
  activeTransfer?.cancel();
  connectBtn.textContent = "Connect";
  otaCharacteristic = undefined;
  bluetoothDevice = undefined;
  if (!uploadCompleted && !activeTransfer?.complete) setProgress(0, "Waiting to start…");
}

function handleNotifications(event) {
  activeTransfer?.handleNotification(event.target.value);
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
      const completed = uploadCompleted || activeTransfer?.complete;
      log(completed ? "Device disconnected after the update." : "Bluetooth connection lost.", completed ? "success" : "error");
      setDisconnectedState(completed ? "Restarting" : "Disconnected");
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
  uploadCompleted = false;
  uploadBtn.disabled = true;
  connectBtn.disabled = true;
  fileInput.disabled = true;
  setProgress(0, "Sending start command…");
  log(`Beginning OTA upload (${firmwareFile.size} bytes).`);
  const transfer = new OtaTransfer(otaCharacteristic, {
    onProgress(percent) { setProgress(percent, `Sending firmware… ${percent}%`); },
    onLegacy() { log("Using paced transfer for older firmware."); },
  });
  activeTransfer = transfer;
  try {
    const firmwareData = new Uint8Array(await firmwareFile.arrayBuffer());
    await transfer.run(firmwareData);
    uploadCompleted = true;
    log("OTA complete. Device will reboot.", "success");
    setProgress(100, "Upload complete");
  } catch (error) {
    log(`Upload failed: ${error.message}`, "error");
    setProgress(0, "Upload failed");
  } finally {
    if (activeTransfer === transfer) activeTransfer = undefined;
    isUploading = false;
    fileInput.disabled = false;
    connectBtn.disabled = false;
    uploadBtn.disabled = transfer.complete || !otaCharacteristic || !bluetoothDevice?.gatt?.connected || !firmwareFile;
  }
}


fileInput.addEventListener("change", (event) => {
  const [file] = event.target.files;
  firmwareFile = file;

  if (file) {
    fileLabel.textContent = file.name;
    fileHelper.textContent = `${file.size.toLocaleString()} bytes selected`;
    uploadBtn.disabled = isUploading || !otaCharacteristic;
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
