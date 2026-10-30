/* BLE transport kept separate from the page so failure paths run under Node. */
class OtaTransfer {
  constructor(characteristic, options = {}) {
    this.characteristic = characteristic;
    this.timeoutMs = options.timeoutMs ?? 15000;
    this.legacyDelayMs = options.legacyDelayMs ?? 50;
    this.onProgress = options.onProgress ?? (() => {});
    this.onLegacy = options.onLegacy ?? (() => {});
    this.pending = null;
    this.error = null;
    this.complete = false;
  }

  cancel(error = new Error("Bluetooth connection lost.")) {
    if (this.complete) return;
    this.error = error;
    this.pending?.reject(error);
  }

  handleNotification(value) {
    if (!value || value.byteLength < 2) return;
    const view = new DataView(value.buffer, value.byteOffset, value.byteLength);
    if (view.getUint8(0) === 0xff) {
      this.cancel(new Error(`Device reported error 0x${view.getUint8(1).toString(16).padStart(2, "0")}`));
      return;
    }
    if (view.getUint8(0) === 0x04 && this.pending?.code !== 0x04) {
      this.cancel(new Error("OTA aborted by device"));
      return;
    }
    if (view.getUint8(1) !== 0 || view.getUint8(0) !== this.pending?.code) return;
    if (this.pending.code === 0x03) this.complete = true;
    this.pending.resolve(view);
  }

  async send(packet, code) {
    if (this.error) throw this.error;
    if (this.pending) throw new Error("An OTA command is already pending.");
    let timer;
    const pending = { code };
    const acknowledged = new Promise((resolve, reject) => Object.assign(pending, { resolve, reject }));
    this.pending = pending; // Register before writing: a notification can arrive first.
    try {
      const written = Promise.resolve().then(() => {
        if (this.error) throw this.error;
        return this.characteristic.writeValueWithResponse(packet);
      });
      const [ , response ] = await Promise.race([
        Promise.all([written, acknowledged]),
        new Promise((_, reject) => {
          timer = setTimeout(() => reject(new Error(`Timed out waiting for OTA command 0x${code.toString(16)}`)), this.timeoutMs);
        }),
      ]);
      if (this.error) throw this.error;
      return response;
    } finally {
      clearTimeout(timer);
      if (this.pending === pending) this.pending = null;
    }
  }

  async abort() {
    // A failed transport may never answer. Bound cleanup independently, and
    // leave firmware's disconnect/inactivity cleanup as the final fallback.
    let timer;
    try {
      await Promise.race([
        this.characteristic.writeValueWithResponse(Uint8Array.of(0x04)),
        new Promise((_, reject) => { timer = setTimeout(() => reject(new Error("Abort timed out")), 2000); }),
      ]);
    } catch (_) {
      // Preserve the original failure.
    } finally {
      clearTimeout(timer);
    }
  }

  async run(firmware) {
    if (!firmware.length || firmware.length > 0xffffffff) throw new Error("Invalid firmware size.");
    const start = new Uint8Array(6);
    start[0] = 0x01;
    new DataView(start.buffer).setUint32(1, firmware.length, true);
    start[5] = 1; // Opt in to acknowledgements after each completed flash write.
    try {
      const response = await this.send(start, 0x01);
      const acknowledgedData = response.byteLength >= 3 && response.getUint8(2) === 1;
      if (!acknowledgedData) this.onLegacy();
      for (let offset = 0; offset < firmware.length; offset += 180) {
        if (this.error) throw this.error;
        const chunk = firmware.subarray(offset, offset + 180);
        const packet = new Uint8Array(chunk.length + 1);
        packet[0] = 0x02;
        packet.set(chunk, 1);
        if (acknowledgedData) {
          const ack = await this.send(packet, 0x02);
          if (ack.byteLength !== 6 || ack.getUint32(2, true) !== offset + chunk.length) {
            throw new Error("Device acknowledged an unexpected firmware offset.");
          }
        } else {
          // Older firmware does not acknowledge DATA. Keep it usable for the
          // first upgrade, pace writes, and still stop on every reported error.
          await Promise.race([
            this.characteristic.writeValueWithResponse(packet),
            new Promise((_, reject) => {
              this.legacyWriteTimer = setTimeout(() => reject(new Error("Firmware write timed out")), this.timeoutMs);
            }),
          ]).finally(() => clearTimeout(this.legacyWriteTimer));
          await new Promise((resolve) => setTimeout(resolve, this.legacyDelayMs));
        }
        if (this.error) throw this.error;
        this.onProgress(Math.min(99, Math.round(((offset + chunk.length) / firmware.length) * 100)));
      }
      await this.send(Uint8Array.of(0x03), 0x03);
    } catch (error) {
      // The device may disconnect after acknowledging END but before the GATT
      // write promise resolves. Its success acknowledgement remains definitive.
      if (this.complete) return;
      this.cancel(error);
      await this.abort();
      throw error;
    }
  }
}

if (typeof module !== "undefined") module.exports = { OtaTransfer };
