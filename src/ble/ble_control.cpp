#include "ble/ble_control.h"
#include "ble/ble.h"
#include "ble/ble_worker.h"
#include "ble/ble_constants.h"
#include "app/AppRuntime.h"
#include "app/AppStateSync.h"
#include "app/PowerManagement.h"
#include "app/ViewState.h"
#include "config/RuntimeSettings.h"
#include "config/StaticColor.h"
#include "config/userPreferences.h"
#include "config/debug_config.h"
#include "core/AdaptiveBrightness.h"
#include "core/PerfTelemetry.h"
#include "effects/strobeEffect.h"
#include <Arduino.h>
#include <string>

// Callback class for handling writes to the command characteristic
class CommandCallbacks : public NimBLECharacteristicCallbacks
{

  // Override the onWrite method
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    // Get the value written by the client
    const auto &rxValue = pCharacteristic->getValue();

    if (rxValue.size() > 0)
    {
      const uint8_t commandCode = rxValue[0]; // Assuming single-byte commands

      Serial.print("Command Characteristic received write, command code: 0x");
      Serial.println(commandCode, HEX);
      if (!bleQueueByte(BleWorkType::Command, pCharacteristic, commandCode))
      {
        Serial.println("BLE command queue full, dropping command.");
      }
    }
    else
    {
      Serial.println("Command Characteristic received empty write.");
    }
  }
} cmdCallbacks;

// New Callback class for handling notifications related to Temperature Logs
class TemperatureLogsCallbacks : public NimBLECharacteristicCallbacks
{
public:
  // Optionally, implement onSubscribe to log when a client subscribes to notifications
  void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override
  {
    std::string str = "Client subscribed to Temperature Logs notifications, Client ID: ";
    str += std::to_string(connInfo.getConnHandle());
    str += " Address: ";
    str += connInfo.getAddress().toString();
    Serial.printf("%s\n", str.c_str());
  }
};
TemperatureLogsCallbacks logsCallbacks;

// Class to handle characteristic callbacks
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
  void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    AppStateLock stateLock;
    uint8_t bleViewValue = static_cast<uint8_t>(currentView);
    pCharacteristic->setValue(&bleViewValue, 1);
    Serial.printf("Read request - returned view: %d\n", bleViewValue);
  }

  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    const auto &value = pCharacteristic->getValue();
    if (value.size() < 1)
    {
      return;
    }
    const uint8_t newView = value[0];
    if (!bleQueueByte(BleWorkType::FaceWrite, pCharacteristic, newView))
    {
      Serial.println("BLE face write queue full, dropping view update.");
    }
  }

  void onStatus(NimBLECharacteristic *pCharacteristic, int code) override
  {
    Serial.printf("Notification/Indication return code: %d, %s\n", code, NimBLEUtils::returnCodeToString(code));
  }

  void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override
  {
    std::string str = "Client ID: ";
    str += std::to_string(connInfo.getConnHandle());
    str += " Address: ";
    str += connInfo.getAddress().toString();
    if (subValue == 0)
    {
      str += " Unsubscribed to ";
    }
    else if (subValue == 1)
    {
      str += " Subscribed to notifications for ";
    }
    else if (subValue == 2)
    {
      str += " Subscribed to indications for ";
    }
    else if (subValue == 3)
    {
      str += " Subscribed to notifications and indications for ";
    }
    str += std::string(pCharacteristic->getUUID());

    Serial.printf("%s\n", str.c_str());
  }
} chrCallbacks;

// Custom characteristic callback class.
class ConfigCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    const auto &value = pCharacteristic->getValue();
    if (value.size() < 4)
    {
      Serial.println("Error: Config payload is not at least 4 bytes.");
      return;
    }
    if (!bleQueuePayload(BleWorkType::ConfigWrite, pCharacteristic, value.data(), value.size()))
    {
      Serial.println("BLE config queue full, dropping configuration update.");
    }
  }
};
// Reuse one instance to avoid heap fragmentation
static ConfigCallbacks configCallbacks;

class BrightnessCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    auto val = pChr->getValue();
    if (val.size() >= 1)
    {
      if (!bleQueueByte(BleWorkType::BrightnessWrite, pChr, static_cast<uint8_t>(val[0])))
      {
        Serial.println("BLE brightness queue full, dropping brightness update.");
      }
    }
  }
};
static BrightnessCallbacks brightnessCallbacks;


class StaticColorCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    const auto &val = pChr->getValue();
    if (val.size() == 0)
    {
      return;
    }

    if (!bleQueuePayload(BleWorkType::StaticColorWrite, pChr, val.data(), val.size()))
    {
      Serial.println("BLE static color queue full, dropping payload.");
    }
  }

  void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    AppStateLock stateLock;
    String hexString = getStaticColorHexString();
    std::string value(hexString.c_str());
    pChr->setValue(value);
  }
};
static StaticColorCallbacks staticColorCallbacks;

class AutoBrightnessFloorCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    NimBLEAttValue val = pChr->getValue();
    if (val.size() < 1)
    {
      return;
    }
    if (!bleQueueByte(BleWorkType::AutoBrightnessFloorWrite, pChr, static_cast<uint8_t>(val[0])))
    {
      Serial.println("BLE floor queue full, dropping floor update.");
    }
  }
};
static AutoBrightnessFloorCallbacks autoBrightnessFloorCallbacks;

static std::string buildStrobeSettingsPayload()
{
  String hexString = getStrobeColorHexString();
  return std::string("{") +
         "\"color\":\"" + std::string(hexString.c_str()) + "\"," +
         "\"speedMs\":" + std::to_string(static_cast<unsigned int>(getStrobeSpeedMs())) +
         "}";
}

class StrobeSettingsCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    (void)connInfo;
    ScopedPerfTelemetryDuration perfScope(PerfDurationId::BleCallback);
    const auto &val = pChr->getValue();
    if (val.size() == 0)
    {
      return;
    }
    if (!bleQueuePayload(BleWorkType::StrobeSettingsWrite, pChr, val.data(), val.size()))
    {
      Serial.println("BLE strobe queue full, dropping payload.");
    }
  }

  void onRead(NimBLECharacteristic *pChr, NimBLEConnInfo &connInfo) override
  {
    AppStateLock stateLock;
    const std::string value = buildStrobeSettingsPayload();
    pChr->setValue(value);
  }
};
static StrobeSettingsCallbacks strobeSettingsCallbacks;

static void handleBleCommandWork(const BleWorkItem &item)
{
  if (item.length < 1)
  {
    return;
  }

  const uint8_t commandCode = item.data[0];
  switch (commandCode)
  {
  case 0x01:
    Serial.println("-> Command: Get Temperature History");
    triggerHistoryTransfer();
    break;
  case 0x02:
    Serial.println("-> Command: Clear History Buffer");
    clearHistoryBuffer();
    break;
  default:
    Serial.print("-> Unknown command code received: 0x");
    Serial.println(commandCode, HEX);
    break;
  }
}

static void handleBleFaceWriteWork(const BleWorkItem &item)
{
  if (item.length) selectUserView(item.data[0]);
}

static void handleBleConfigWriteWork(const BleWorkItem &item)
{
  if (item.length < 4)
  {
    Serial.println("Error: Config payload is not at least 4 bytes.");
    return;
  }

#if defined(DEBUG_BLE)
  Serial.println("Received configuration update:");
#endif
  const bool oldAutoBrightnessEnabled = autoBrightnessEnabled;
  const bool oldAuroraMode = auroraModeEnabled;
  const bool oldStaticColorMode = staticColorModeEnabled;
  const bool oldMouthMicBrightnessOverride = mouthMicBrightnessOverrideEnabled;

  autoBrightnessEnabled = (item.data[0] != 0);
  accelerometerEnabled = (item.data[1] != 0);
  sleepModeEnabled = (item.data[2] != 0);
  auroraModeEnabled = (item.data[3] != 0);
  if (item.length >= 5)
  {
    staticColorModeEnabled = (item.data[4] != 0);
  }
  else
  {
    staticColorModeEnabled = false;
#if defined(DEBUG_BLE)
    Serial.println("Static color flag not provided, defaulting to disabled.");
#endif
  }
  if (item.length >= 6)
  {
    mouthMicBrightnessOverrideEnabled = (item.data[5] != 0);
  }

  const bool oldWaveshareBoost = waveshareBrightnessBoost;

  // Bytes 6–8. A 6-byte write does not change these.
  if (item.length >= 7)
  {
    disableBleIndicatorLight = (item.data[6] != 0);
  }
  if (item.length >= 8)
  {
    disableBleStatusIcon = (item.data[7] != 0);
  }
  if (item.length >= 9)
  {
    waveshareBrightnessBoost = (item.data[8] != 0);
  }
    // Byte 9. A short packet leaves the saved value alone.
  if (item.length >= 10)
  {
    matrixRainInsteadOfPlasma = (item.data[9] != 0);
  }
  setMatrixRainInsteadOfPlasma(matrixRainInsteadOfPlasma);
  if (item.length >= 11)
  {
    boopMode = item.data[10];
    if (boopMode > 3)
    {
      boopMode = 0;
    }
  }
  setBoopMode(boopMode);
  if (staticColorModeEnabled)
  {
    auroraModeEnabled = false;
  }

  setAutoBrightness(autoBrightnessEnabled);
  setAccelerometerEnabled(accelerometerEnabled);
  setSleepMode(sleepModeEnabled);
  setAuroraMode(auroraModeEnabled);
  setStaticColorMode(staticColorModeEnabled);
  setMouthMicBrightnessOverride(mouthMicBrightnessOverrideEnabled);
  setDisableBleIndicatorLight(disableBleIndicatorLight);
  setDisableBleStatusIcon(disableBleStatusIcon);
  setWaveshareBrightnessBoost(waveshareBrightnessBoost);

#if defined(DEBUG_BLE)
  Serial.print("  Auto Brightness: ");
  Serial.println(autoBrightnessEnabled ? "Enabled" : "Disabled");
  Serial.print("  Accelerometer:   ");
  Serial.println(accelerometerEnabled ? "Enabled" : "Disabled");
  Serial.print("  Sleep Mode:      ");
  Serial.println(sleepModeEnabled ? "Enabled" : "Disabled");
  Serial.print("  Aurora Mode:     ");
  Serial.println(auroraModeEnabled ? "Enabled" : "Disabled");
  Serial.print("  Static Color:    ");
  Serial.println(staticColorModeEnabled ? "Enabled" : "Disabled");
  Serial.print("  Mouth Mic Max:   ");
  Serial.println(mouthMicBrightnessOverrideEnabled ? "Enabled" : "Disabled");
  Serial.print("  BLE indicator:   ");
  Serial.println(disableBleIndicatorLight ? "Disabled" : "Enabled");
  Serial.print("  BLE status icon: ");
  Serial.println(disableBleStatusIcon ? "Hidden" : "Shown");
  Serial.print("  Waveshare boost: ");
  Serial.println(waveshareBrightnessBoost ? "Enabled" : "Disabled");
  Serial.print("  Matrix fill:     ");
  Serial.println(matrixRainInsteadOfPlasma ? "Matrix rain" : "Plasma");
  Serial.print("  Boop mode:       ");
  Serial.println(boopMode);
#endif

  if (item.characteristic != nullptr)
  {
           const uint8_t canonicalConfig[11] = {
        static_cast<uint8_t>(autoBrightnessEnabled ? 1 : 0),
        static_cast<uint8_t>(accelerometerEnabled ? 1 : 0),
        static_cast<uint8_t>(sleepModeEnabled ? 1 : 0),
        static_cast<uint8_t>(auroraModeEnabled ? 1 : 0),
        static_cast<uint8_t>(staticColorModeEnabled ? 1 : 0),
        static_cast<uint8_t>(mouthMicBrightnessOverrideEnabled ? 1 : 0),
        static_cast<uint8_t>(disableBleIndicatorLight ? 1 : 0),
        static_cast<uint8_t>(disableBleStatusIcon ? 1 : 0),
        static_cast<uint8_t>(waveshareBrightnessBoost ? 1 : 0),
        static_cast<uint8_t>(matrixRainInsteadOfPlasma ? 1 : 0),
        static_cast<uint8_t>(boopMode)};
    item.characteristic->setValue(canonicalConfig, sizeof(canonicalConfig));
  }

  applyConfigOptions();
  if (oldAutoBrightnessEnabled != autoBrightnessEnabled)
  {
    if (autoBrightnessEnabled)
    {
#if defined(DEBUG_BLE)
      Serial.println("Auto brightness has been ENABLED. Adaptive logic will take over.");
#endif
    }
    else
    {
#if defined(DEBUG_BLE)
      Serial.println("Auto brightness has been DISABLED. Applying user-set brightness.");
      Serial.printf("Applied manual brightness: %u\n", userBrightness);
#endif
      updateGlobalBrightnessScale(userBrightness);
      syncBrightnessState(userBrightness);
    }
  }

  if (oldAuroraMode != auroraModeEnabled || oldStaticColorMode != staticColorModeEnabled)
  {
    facePlasmaDirty = true;
    requestDisplayRefresh();
  }
    if (oldMouthMicBrightnessOverride != mouthMicBrightnessOverrideEnabled)
  {
    requestDisplayRefresh();
    notifyBleTask();
  }

  // Software gain changes apply through the rendering task without rebooting.
  if (oldWaveshareBoost != waveshareBrightnessBoost)
  {
    updateGlobalBrightnessScale(static_cast<uint8_t>(constrain(getLastAppliedBrightness(), 0, 255)));
  }
}

// One byte, 0–255. Higher value raises the auto-brightness floor.
static void handleBleAutoBrightnessFloorWriteWork(const BleWorkItem &item)
{
  if (item.length < 1)
  {
    return;
  }
  autoBrightnessFloor = item.data[0];
  setAutoBrightnessFloor(autoBrightnessFloor);
  Serial.printf("Auto brightness floor set to %u\n", autoBrightnessFloor);
  if (item.characteristic != nullptr)
  {
    item.characteristic->setValue(&autoBrightnessFloor, 1);
  }
}

static void handleBleBrightnessWriteWork(const BleWorkItem &item)
{
  if (item.length < 1)
  {
    return;
  }

  userBrightness = item.data[0];
  setUserBrightness(userBrightness);
#if DEBUG_BRIGHTNESS
  Serial.printf("Brightness target set to %u\n", userBrightness);
#endif
}

static void handleBleStaticColorWriteWork(const BleWorkItem &item)
{
  if (item.characteristic == nullptr || item.length == 0)
  {
    return;
  }

  std::string normalized;
  if (!applyStaticColorBytes(item.data, item.length, true, &normalized))
  {
    Serial.println("Invalid static color payload received over BLE.");
    return;
  }

  item.characteristic->setValue(normalized);
  item.characteristic->notify();
#if DEBUG_MODE
  Serial.printf("Static color updated to #%s\n", normalized.c_str());
#endif
}

static void handleBleStrobeSettingsWriteWork(const BleWorkItem &item)
{
  if (item.characteristic == nullptr || item.length < 2)
  {
    return;
  }

  constexpr uint8_t kOpcodeColor = 0x01;
  constexpr uint8_t kOpcodeSpeed = 0x02;

  const uint8_t opcode = item.data[0];
  const uint8_t *payload = item.data + 1;
  const size_t payloadLength = item.length - 1;

  if (opcode == kOpcodeColor)
  {
    std::string normalized;
    if (!applyStrobeColorBytes(payload, payloadLength, true, &normalized))
    {
      Serial.println("Invalid strobe color payload received over BLE.");
      return;
    }
#if DEBUG_MODE
    Serial.printf("Strobe color updated to #%s\n", normalized.c_str());
#endif
  }
  else if (opcode == kOpcodeSpeed)
  {
    uint16_t speedMs = 0;
    if (!applyStrobeSpeedBytes(payload, payloadLength, true, &speedMs))
    {
      Serial.println("Invalid strobe speed payload received over BLE.");
      return;
    }
#if DEBUG_MODE
    Serial.printf("Strobe speed updated to %u ms\n", static_cast<unsigned int>(speedMs));
#endif
  }
  else
  {
    Serial.printf("Invalid strobe settings opcode received over BLE: 0x%02X\n", opcode);
    return;
  }

  const std::string response = buildStrobeSettingsPayload();
  item.characteristic->setValue(response);
  item.characteristic->notify();
}

void bleHandleControlWork(const BleWorkItem &item)
{
  // History transfers never hold the render/state lock while waiting on BLE.
  if (item.type == BleWorkType::Command)
  {
    handleBleCommandWork(item);
    return;
  }
  AppStateLock stateLock;
    switch (item.type)
    {
    case BleWorkType::Command:
      handleBleCommandWork(item);
      break;
    case BleWorkType::FaceWrite:
      handleBleFaceWriteWork(item);
      break;
    case BleWorkType::ConfigWrite:
      handleBleConfigWriteWork(item);
      break;
    case BleWorkType::BrightnessWrite:
      handleBleBrightnessWriteWork(item);
      break;
    case BleWorkType::AutoBrightnessFloorWrite:
      handleBleAutoBrightnessFloorWriteWork(item);
      break;
    case BleWorkType::StaticColorWrite:
      handleBleStaticColorWriteWork(item);
      break;
    case BleWorkType::StrobeSettingsWrite:
      handleBleStrobeSettingsWriteWork(item);
      break;
    case BleWorkType::ScrollWrite:
      handleScrollWritePayload(item.characteristic, item.data, item.length);
      break;
    default:
      break;
    }
}

class DescriptorCallbacks : public NimBLEDescriptorCallbacks
{
  void onWrite(NimBLEDescriptor *pDescriptor, NimBLEConnInfo &connInfo) override
  {
    std::string dscVal = pDescriptor->getValue();
    Serial.printf("Descriptor written value: %s\n", dscVal.c_str());
  }
  void onRead(NimBLEDescriptor *pDescriptor, NimBLEConnInfo &connInfo) override
  {
    Serial.printf("%s Descriptor read\n", pDescriptor->getUUID().toString().c_str());
  }
} dscCallbacks;


void setupBleControl()
{
  ///////////////////// Setup BLE ////////////////////////
  Serial.println("Initializing BLE...");
  // NimBLEDevice::init("LumiFur_Controller");
  NimBLEDevice::init("LF-052618");
  // NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Power level 9 (highest) for best range
  // NimBLEDevice::setPower(ESP_PWR_LVL_P21, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  NimBLEDevice::setPower(ESP_PWR_LVL_P9, NimBLETxPowerType::All); // Power level 21 (highest) for best range
  // NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC);
  NimBLEDevice::setSecurityAuth(true, true, true);
  // Keep default passkey so onPassKeyDisplay supplies a dynamic code.
  NimBLEDevice::setSecurityPasskey(123456);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);
  pServer->advertiseOnDisconnect(false);

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  pInfoCharacteristic = pService->createCharacteristic(
      INFO_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::NOTIFY
    );

  // Construct JSON string
  std::string jsonInfo = std::string("{") +
                         "\"fw\":\"" + std::string(FIRMWARE_VERSION) + "\"," +
                         "\"commit\":\"" + std::string(GIT_COMMIT) + "\"," +
                         "\"branch\":\"" + std::string(GIT_BRANCH) + "\"," +
                         "\"build\":\"" + std::string(BUILD_DATE) + " " + BUILD_TIME + "\"," +
                         "\"model\":\"" + std::string(DEVICE_MODEL) + "\"," +
                         "\"compat\":\"" + std::string(APP_COMPAT_VERSION) + "\"," +
                         "\"id\":\"" + NimBLEDevice::getAddress().toString() + "\"" +
                         "}";

  pInfoCharacteristic->setValue(jsonInfo.c_str());
  Serial.println("Device Info Service started");
  Serial.println(jsonInfo.c_str());

  // Face control characteristic with encryption
  pFaceCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::NOTIFY
      // NIMBLE_PROPERTY::READ_ENC  // only allow reading if paired / encrypted
      // NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
  );

  // Set initial view value
  uint8_t bleViewValue = static_cast<uint8_t>(currentView);
  pFaceCharacteristic->setValue(&bleViewValue, 1);
  pFaceCharacteristic->setCallbacks(&chrCallbacks);

  pCommandCharacteristic = pService->createCharacteristic(
      COMMAND_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::WRITE_ENC |
      NIMBLE_PROPERTY::NOTIFY
    );
  pCommandCharacteristic->setCallbacks(&cmdCallbacks);
  Serial.print("Command Characteristic created, UUID: ");
  Serial.println(pCommandCharacteristic->getUUID().toString().c_str());
  Serial.print("Properties: ");
  Serial.println(pCommandCharacteristic->getProperties()); // Print properties as integer

  // Temperature characteristic with encryption
  pTemperatureCharacteristic =
      pService->createCharacteristic(
          TEMPERATURE_CHARACTERISTIC_UUID,
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::NOTIFY
          //NIMBLE_PROPERTY::READ_ENC
      );

  // Create a characteristic for configuration.
  pConfigCharacteristic = pService->createCharacteristic(
      CONFIG_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::WRITE_ENC |
      //NIMBLE_PROPERTY::WRITE_NR |
      NIMBLE_PROPERTY::NOTIFY
    );

  // Set the callback to handle writes.
  pConfigCharacteristic->setCallbacks(&configCallbacks);

  // Optionally, set an initial value.
  uint8_t initValue[11] = {
      static_cast<uint8_t>(autoBrightnessEnabled ? 1 : 0),             // 0
      static_cast<uint8_t>(accelerometerEnabled ? 1 : 0),              // 1
      static_cast<uint8_t>(sleepModeEnabled ? 1 : 0),                  // 2
      static_cast<uint8_t>(auroraModeEnabled ? 1 : 0),                 // 3
      static_cast<uint8_t>(staticColorModeEnabled ? 1 : 0),            // 4
      static_cast<uint8_t>(mouthMicBrightnessOverrideEnabled ? 1 : 0), // 5
      static_cast<uint8_t>(disableBleIndicatorLight ? 1 : 0),          // 6
      static_cast<uint8_t>(disableBleStatusIcon ? 1 : 0),              // 7
      static_cast<uint8_t>(waveshareBrightnessBoost ? 1 : 0),          // 8
      static_cast<uint8_t>(matrixRainInsteadOfPlasma ? 1 : 0),         // 9
      static_cast<uint8_t>(boopMode)};                                 // 10

  pConfigCharacteristic->setValue(initValue, sizeof(initValue));
  if (pConfigCharacteristic != nullptr)
  {
    Serial.println("Config Characteristic created");
  }
  else
  {
    Serial.println("Error: Failed to create config characteristic.");
  }

  //
  pTemperatureLogsCharacteristic = pService->createCharacteristic(
      TEMPERATURE_LOGS_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::NOTIFY);
  // pTemperatureLogsCharacteristic->setCallbacks(&chrCallbacks);
  Serial.println("Command Characteristic created");
  pTemperatureLogsCharacteristic->setCallbacks(&logsCallbacks); // If using new logsCallbacks
  Serial.print("Temperature Logs Characteristic created, UUID: ");
  Serial.println(pTemperatureLogsCharacteristic->getUUID().toString().c_str());
  Serial.print("Properties: ");
  Serial.println(pTemperatureLogsCharacteristic->getProperties()); // Print properties as integer

  // Set up descriptors
  NimBLE2904 *faceDesc = pFaceCharacteristic->create2904();
  faceDesc->setFormat(NimBLE2904::FORMAT_UINT8);
  faceDesc->setUnit(0x2700); // Unit-less number
  faceDesc->setCallbacks(&dscCallbacks);
  // Add user description descriptor
  NimBLEDescriptor *pDesc =
      pFaceCharacteristic->createDescriptor(
          "2901",
          NIMBLE_PROPERTY::READ,
          20);
  pDesc->setValue("Face Control");

  // Assuming pTemperatureCharacteristic is already created:
  NimBLEDescriptor *tempDesc =
      pTemperatureCharacteristic->createDescriptor(
          "2901", // Standard UUID for the Characteristic User Description
          NIMBLE_PROPERTY::READ,
          20 // Maximum length for the descriptor's value
      );
  tempDesc->setValue("Temperature Sensor");
  NimBLE2904 *tempFormat = pTemperatureCharacteristic->create2904();
  tempFormat->setFormat(NimBLE2904::FORMAT_UINT8);
  tempFormat->setUnit(0x272F); // For example, 0x272F might represent degrees Celsius per specific BLE specifications

  pBrightnessCharacteristic = pService->createCharacteristic(
      BRIGHTNESS_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          // NIMBLE_PROPERTY::NOTIFY |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::WRITE_NR);
  pBrightnessCharacteristic->setCallbacks(&brightnessCallbacks);
  // initialize with current brightness

  
  NimBLECharacteristic *pAutoBrightnessFloorCharacteristic = nullptr;
  pAutoBrightnessFloorCharacteristic = pService->createCharacteristic(
    AUTO_BRIGHTNESS_FLOOR_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pAutoBrightnessFloorCharacteristic->setCallbacks(&autoBrightnessFloorCallbacks);

  pOtaCharacteristic = pService->createCharacteristic(
      OTA_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE_ENC |
        NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::NOTIFY);
  pOtaCharacteristic->setCallbacks(&otaCallbacks);
  NimBLEDescriptor *otaDesc = pOtaCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  otaDesc->setValue("OTA Control");
  pBrightnessCharacteristic->setValue(&userBrightness, 1);
  pAutoBrightnessFloorCharacteristic->setValue(&autoBrightnessFloor, 1);

  setupAdaptiveBrightness();

  // Create lux characteristic (read-only with notifications)
  pLuxCharacteristic = pService->createCharacteristic(
      LUX_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::NOTIFY);

  // Initialize with current lux value
  uint16_t initialLux = getAmbientLuxU16();
  uint8_t luxBytes[2] = {
      static_cast<uint8_t>(initialLux & 0xFF),
      static_cast<uint8_t>((initialLux >> 8) & 0xFF)};
  pLuxCharacteristic->setValue(luxBytes, 2);
  // Add descriptor for lux characteristic
  NimBLEDescriptor *luxDesc = pLuxCharacteristic->createDescriptor(
      "2901", // Standard UUID for Characteristic User Description
      NIMBLE_PROPERTY::READ,
      20 // Maximum length for the descriptor's value
  );
  luxDesc->setValue("Ambient Light Sensor");
  NimBLE2904 *luxFormat = pLuxCharacteristic->create2904();
  luxFormat->setFormat(NimBLE2904::FORMAT_UINT16);
  luxFormat->setUnit(0x2731); // 0x2731 is the standard unit for Illuminance (lux)

  pScrollTextCharacteristic = pService->createCharacteristic(
      SCROLL_TEXT_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC | // Require encryption for writes.
          NIMBLE_PROPERTY::NOTIFY);
  pScrollTextCharacteristic->setCallbacks(&scrollTextCallbacks);

  // Set initial value (current text)
  pScrollTextCharacteristic->setValue(txt);

  // Add descriptor
  NimBLEDescriptor *scrollTextDesc = pScrollTextCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  scrollTextDesc->setValue("Scrolling Text");

  Serial.println("Scroll Text Characteristic created");

  pStaticColorCharacteristic = pService->createCharacteristic(
      STATIC_COLOR_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::READ_ENC |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::NOTIFY);
  pStaticColorCharacteristic->setCallbacks(&staticColorCallbacks);
  String initialColorHex = getStaticColorHexString();
  pStaticColorCharacteristic->setValue(initialColorHex.c_str());
  NimBLEDescriptor *staticColorDesc = pStaticColorCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  staticColorDesc->setValue("Static Color");
  Serial.println("Static Color Characteristic created");

  pStrobeSettingsCharacteristic = pService->createCharacteristic(
      STROBE_SETTINGS_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::READ_ENC |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::WRITE_ENC |
          NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::NOTIFY);
  pStrobeSettingsCharacteristic->setCallbacks(&strobeSettingsCallbacks);
  const std::string initialStrobeSettings = buildStrobeSettingsPayload();
  pStrobeSettingsCharacteristic->setValue(initialStrobeSettings);
  NimBLEDescriptor *strobeSettingsDesc = pStrobeSettingsCharacteristic->createDescriptor(
      "2901",
      NIMBLE_PROPERTY::READ,
      20);
  strobeSettingsDesc->setValue("Strobe Settings");
  Serial.println("Strobe Settings Characteristic created");

  // nimBLEService* pBaadService = pServer->createService("BAAD");
  // Services are started when the GATT server starts during advertising.

  /** Create an advertising instance and add the services to the advertised data */
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName("LumiFur_Controller");
  // pAdvertising->setAppearance(0);         // Appearance value (0 is generic)
  pAdvertising->enableScanResponse(true); // Enable scan response to include more data
  pAdvertising->addServiceUUID(pService->getUUID());
  refreshBleAdvertising();

  Serial.println(shouldBleAdvertise() ? "BLE setup complete - advertising started"
                                      : "BLE setup complete - waiting for pairing mode");

}
