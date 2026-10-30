const { test } = require("node:test");
const assert = require("node:assert/strict");
const { OtaTransfer } = require("../ota-transfer.js");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

function fixture(options = {}) {
  const commands = [];
  let received = 0;
  let transfer;
  const notify = (...bytes) => {
    // Exercise DataView byteOffset: Bluetooth data need not start at buffer[0].
    const backing = Uint8Array.of(99, ...bytes, 99);
    transfer.handleNotification(new DataView(backing.buffer, 1, bytes.length));
  };
  const characteristic = {
    async writeValueWithResponse(value) {
      const bytes = new Uint8Array(value.buffer || value, value.byteOffset || 0, value.byteLength);
      const command = bytes[0];
      commands.push(command);
      if (options.write) return options.write({ command, bytes, notify, transfer });
      if (command === 1) notify(1, 0, 1);
      if (command === 2) {
        received += bytes.length - 1;
        notify(2, 0, received & 255, (received >> 8) & 255, (received >> 16) & 255, (received >> 24) & 255);
      }
      if (command === 3) notify(3, 0);
    },
  };
  transfer = new OtaTransfer(characteristic, { timeoutMs: 20, legacyDelayMs: 0, ...options });
  return { transfer, commands, notify };
}

test("acknowledged transfer validates offsets and completes", async () => {
  const progress = [];
  const { transfer, commands } = fixture({ onProgress: (p) => progress.push(p) });
  await transfer.run(new Uint8Array(400));
  assert.deepEqual(commands, [1, 2, 2, 2, 3]);
  assert.deepEqual(progress, [45, 90, 99]);
  assert.equal(transfer.complete, true);
});

test("no DATA is sent before START acknowledgement", async () => {
  const { transfer, commands } = fixture({ write() {} });
  await assert.rejects(transfer.run(new Uint8Array(400)), /Timed out/);
  assert.deepEqual(commands, [1, 4]);
});

test("device START rejection stops and aborts", async () => {
  const { transfer, commands } = fixture({ write({ command, notify }) {
    if (command === 1) notify(255, 3);
  } });
  await assert.rejects(transfer.run(new Uint8Array(400)), /Device reported error 0x03/);
  assert.deepEqual(commands, [1, 4]);
});

test("queue-full DATA rejection stops without sending END", async () => {
  const { transfer, commands } = fixture({ write({ command, notify }) {
    if (command === 1) notify(1, 0, 1);
    if (command === 2) notify(255, 10);
  } });
  await assert.rejects(transfer.run(new Uint8Array(400)), /0x0a/);
  assert.deepEqual(commands, [1, 2, 4]);
});

test("missing DATA acknowledgement times out and aborts", async () => {
  const { transfer, commands } = fixture({ write({ command, notify }) {
    if (command === 1) notify(1, 0, 1);
  } });
  await assert.rejects(transfer.run(new Uint8Array(400)), /Timed out/);
  assert.deepEqual(commands, [1, 2, 4]);
});

test("incorrect DATA offset is rejected", async () => {
  const { transfer, commands } = fixture({ write({ command, notify }) {
    if (command === 1) notify(1, 0, 1);
    if (command === 2) notify(2, 0, 1, 0, 0, 0);
  } });
  await assert.rejects(transfer.run(new Uint8Array(400)), /unexpected firmware offset/);
  assert.deepEqual(commands, [1, 2, 4]);
});

test("a never-resolving GATT write has an operation deadline", async () => {
  const { transfer } = fixture({ write({ command, notify }) {
    if (command === 1) { notify(1, 0, 1); return new Promise(() => {}); }
  } });
  await assert.rejects(transfer.run(new Uint8Array(10)), /Timed out/);
});

test("disconnect during DATA cancels the transfer", async () => {
  const { transfer, commands } = fixture({ write({ command, notify, transfer }) {
    if (command === 1) notify(1, 0, 1);
    if (command === 2) transfer.cancel();
  } });
  await assert.rejects(transfer.run(new Uint8Array(400)), /connection lost/);
  assert.deepEqual(commands, [1, 2, 4]);
});

test("acknowledged END remains successful if reboot interrupts its GATT write", async () => {
  const { transfer } = fixture({ write({ command, notify }) {
    if (command === 1) notify(1, 0, 1);
    if (command === 2) notify(2, 0, 10, 0, 0, 0);
    if (command === 3) { notify(3, 0); throw new Error("Disconnected"); }
  } });
  await transfer.run(new Uint8Array(10));
  assert.equal(transfer.complete, true);
});

test("legacy firmware remains supported with paced DATA writes", async () => {
  let legacy = false;
  const { transfer, commands } = fixture({ onLegacy() { legacy = true; }, write({ command, notify }) {
    if (command === 1) notify(1, 0);
    if (command === 3) notify(3, 0);
  } });
  await transfer.run(new Uint8Array(400));
  assert.equal(legacy, true);
  assert.deepEqual(commands, [1, 2, 2, 2, 3]);
});

test("legacy device errors also stop DATA and abort", async () => {
  const { transfer, commands } = fixture({ write({ command, notify }) {
    if (command === 1) notify(1, 0);
    if (command === 2) notify(255, 6);
  } });
  await assert.rejects(transfer.run(new Uint8Array(400)), /0x06/);
  assert.deepEqual(commands, [1, 2, 4]);
});

test("invalid firmware sizes never start an OTA session", async () => {
  const { transfer, commands } = fixture();
  await assert.rejects(transfer.run(new Uint8Array(0)), /Invalid firmware size/);
  assert.deepEqual(commands, []);
});

function pageFixture(write) {
  const elements = new Map();
  const context = {
    OtaTransfer, Uint8Array, DataView, ArrayBuffer, setTimeout, clearTimeout,
    navigator: { bluetooth: {} }, window: { isSecureContext: true, addEventListener() {} },
    document: {
      getElementById(id) {
        if (!elements.has(id)) elements.set(id, {
          disabled: false, style: {}, classList: { add() {}, remove() {} },
          addEventListener() {}, appendChild() {},
        });
        return elements.get(id);
      },
      createElement() { return {}; },
    },
  };
  vm.createContext(context);
  vm.runInContext(fs.readFileSync(path.join(__dirname, "../app.js"), "utf8"), context);
  let received = 0;
  function notify(...bytes) {
    context.notification = new DataView(Uint8Array.from(bytes).buffer);
    vm.runInContext("handleNotifications({target:{value:notification}})", context);
  }
  context.characteristic = { async writeValueWithResponse(packet) {
    if (write) return write(packet[0], notify);
    if (packet[0] === 1) notify(1, 0, 1);
    if (packet[0] === 2) {
      received += packet.length - 1;
      notify(2, 0, received & 255, received >> 8, 0, 0);
    }
    if (packet[0] === 3) notify(3, 0);
  } };
  vm.runInContext(`
    bluetoothDevice={gatt:{connected:true}};
    otaCharacteristic=characteristic;
    firmwareFile={size:10,async arrayBuffer(){return new ArrayBuffer(10)}};
  `, context);
  return { elements, context, run: (code) => vm.runInContext(code, context) };
}

test("page keeps completion visible after the reboot disconnects", async () => {
  const { elements, run } = pageFixture();
  await run("upload()");
  run('setDisconnectedState("Restarting")');
  assert.equal(elements.get("progress-label").textContent, "Upload complete");
  assert.equal(elements.get("upload-btn").disabled, true);
});

test("page prevents another upload until error cleanup has settled", async () => {
  let finishAbort;
  const commands = [];
  const { elements, run } = pageFixture((command, notify) => {
    commands.push(command);
    if (command === 1) notify(255, 3);
    if (command === 4) return new Promise((resolve) => { finishAbort = resolve; });
  });
  const uploading = run("upload()");
  while (!finishAbort) await new Promise((resolve) => setImmediate(resolve));
  assert.equal(run("isUploading"), true);
  assert.equal(elements.get("upload-btn").disabled, true);
  assert.equal(elements.get("firmware-file").disabled, true);
  await run("upload()");
  assert.deepEqual(commands, [1, 4]);
  finishAbort();
  await uploading;
  assert.equal(run("isUploading"), false);
  assert.equal(elements.get("progress-label").textContent, "Upload failed");
});
