#pragma once

#include "BasicTypes.h"

#include <QObject>
#include <QtEndian>
#include <cstdint>

namespace sphero {

template <typename PACKET>
QByteArray packetToByteArray(const PACKET &packet)
{
    QByteArray ret(reinterpret_cast<const char*>(&packet), sizeof(PACKET));
    qToBigEndian<char*>(ret.data(), ret.size(), ret.data());
    return ret;
}

#pragma pack(push,1)

struct CommandPacketHeader {
    enum TimeoutHandling : uint8_t {
        KeepTimeout = 0,
        ResetTimeout = 1 << 0
    };

    enum SynchronousType : uint8_t {
        Asynchronous = 0,
        Synchronous = 1 << 1
    };

    enum CommandTarget : uint8_t {
        Internal = 0x00,
        Bootloader = 0x01,
        HardwareControl = 0x02,
    };

    enum BootloaderCommand : uint8_t {
        BeginReflash = 0x02,
        HereIsPage = 0x03,
        JumpToMain = 0x04,
        IsPageBlank = 0x05,
        EraseUserConfig = 0x06
    };

    enum InternalCommand : uint8_t {
        Ping = 0x01,
        GetVersion = 0x02,
        SetBtName = 0x10,
        GetBtName = 0x11,
        SetAutoReconnect = 0x12,
        GetAutoReconnect = 0x13,
        GetPwrState = 0x20,
        SetPwrNotify = 0x21,
        Sleep = 0x22,
        GetVoltageTrip = 0x23,
        SetVoltageTrip = 0x24,
        SetInactiveTimeout = 0x22,
        GotoBl = 0x30,
        RunL1Diags = 0x40,
        RunL2Diags = 0x41,
        ClearCounters = 0x42,
        AssignCounter = 0x50,
        PollTimes = 0x51
    };

    enum HardwareCommand : uint8_t {
        SetHeading = 0x01,
        SetStabilization = 0x02,
        SetRotationRate = 0x03,
        SetAppConfigBlk = 0x04,
        GetAppConfigBlk = 0x05,
        SelfLevel = 0x09,
        SetDataStreaming = 0x11,
        ConfigureCollisionDetection = 0x12,
        ConfigureLocator = 0x13,
        GetLocatorData = 0x15,
        SetRGBLed = 0x20,
        SetBackLED = 0x21,
        GetRGBLed = 0x22,
        Roll = 0x30,
        Boost = 0x31,
        RawMotorValues = 0x33,
        SetMotionTimeout = 0x34,
        SetOptionFlags = 0x35,
        GetOptionFlags = 0x36,
        SetNonPersistentOptionFlags = 0x37,
        GetNonPersistentOptionFlags = 0x38,
        GetConfigurationBlock = 0x40,
        SetDeviceMode = 0x42,
        SetConfigurationBlock = 0x43,
        GetDeviceMode = 0x44,
        SetFactoryDeviceMode = 0x45,
        GetSSB = 0x46,
        SetSSB = 0x47,
        RefillBank = 0x48,
        BuyConsumable = 0x49,
        AddXp = 0x4C,
        LevelUpAttribute = 0x4D,
        RunMacro = 0x50,
        SaveTempMacro = 0x51,
        SaveMacro = 0x52,
        DelMacro = 0x53,
        GetMacroStatus = 0x56,
        SetMacroStatus = 0x57,
        SaveTempMacroChunk = 0x58,
        InitMacroExecutive = 0x54,
        AbortMacro = 0x55,
        GetConfigBlock = 0x40,
        OrbBasicEraseStorage = 0x60,
        OrbBasicAppendFragment = 0x61,
        OrbBasicExecute = 0x62,
        OrbBasicAbort = 0x63,
        OrbBasicCommitRamProgramToFlash = 0x65,
        RemoveCores = 0x71,
        SetSSBUnlockFlagsBlock = 0x72,
        ResetSoulBlock = 0x73,
        ReadOdometer = 0x75,
        WritePersistentPage = 0x90
    };
    enum SoulCommands : uint8_t {
        ReadSoulBlock = 0xf0,
        SoulAddXP = 0xf1
    };

    const uint8_t magic = 0xFF;
    uint8_t flags = 0xFC;

    uint8_t deviceID = 0;
    uint8_t commandID = 0;
    uint8_t sequenceNumber = 0;
    uint8_t dataLength = 0;
};

struct RotateCommandPacket
{
    float rate;
};

struct SetOptionsCommandPacket
{
    enum Options : uint32_t {
        /// Prevent Sphero from going to sleep when placed in the charger and connected over Bluetooth
        PreventSleepInCharger = 1 << 0,

        /// Enable Vector Drive, when Sphero is stopped and a new roll command is issued */
        EnableVectorDrive = 1 << 1,

        /// Disable self-leveling when Sphero is inserted into the charger
        DisableSelfLevelInCharger = 1 << 2,

        /// Force the tail LED always on
        TailLightAlwaysOn = 1 << 3,

        /// Enable motion timeouts, DID 0x02, CID 0x34
        EnableMotionTimeout = 1 << 4,

        DemoMode = 1 << 5,

        LightDoubleTap = 1 << 6,
        HeavyDoubleTap = 1 << 7,
        GyroMaxAsync = 1 << 8, // new in firmware 1.47 (sphero)
        EnableSoul   = 1 << 9,
        SlewRawMotors  = 1 << 10,
    };

    uint32_t optionsBitmask = 0;

    // Use the options as a bitmask, e. g. `create(Options::PreventSleepInCharger | Options::DemoMode);`
    static QByteArray create(const uint32_t options) {
        SetOptionsCommandPacket def;
        def.optionsBitmask = options;
        return packetToByteArray(def);
    }

    // don't use this, you get unreadable code
    // Only supports the documented options from the SDK
    static QByteArray create(const bool preventSleepInCharger,
                             const bool enableVectorDrive,
                             const bool disableSelfLevelInCharger,
                             const bool tailLightAlwaysOn,
                             const bool enableMotionTimeout
                             )
    {
        SetOptionsCommandPacket def;

        if (preventSleepInCharger) def.optionsBitmask |= PreventSleepInCharger;
        if (enableVectorDrive) def.optionsBitmask |= EnableVectorDrive;
        if (disableSelfLevelInCharger) def.optionsBitmask |= DisableSelfLevelInCharger;
        if (tailLightAlwaysOn) def.optionsBitmask |= TailLightAlwaysOn;
        if (enableMotionTimeout) def.optionsBitmask |= EnableMotionTimeout;

        return packetToByteArray(def);
    }
};

struct DataStreamingCommandPacket
{
    static constexpr uint32_t deviceId = CommandPacketHeader::HardwareControl;
    static constexpr uint32_t commandId = CommandPacketHeader::SetDataStreaming;

    enum SourceMask : uint32_t {
        NoMask = 0x00000000,
        LeftMotorBackEMFFiltered = 0x00000060,
        RightMotorBackEMFFiltered = 0x00000060,

        MagnetometerZFiltered = 0x00000080,
        MagnetometerYFiltered = 0x00000100,
        MagnetometerXFiltered = 0x00000200,

        GyroZFiltered = 0x00000400,
        GyroYFiltered = 0x00000800,
        GyroXFiltered = 0x00001000,

        AccelerometerZFiltered = 0x00002000,
        AccelerometerYFiltered = 0x00004000,
        AccelerometerXFiltered = 0x00008000,

        IMUYawAngleFiltered = 0x00010000,
        IMURollAngleFiltered = 0x00020000,
        IMUPitchAngleFiltered = 0x00040000,

        LeftMotorBackEMFRaw =  0x00600000,
        RightMotorBackEMFRaw = 0x00600000,

        GyroFilteredAll = 0x00001C00,
        IMUAnglesFilteredAll = 0x00070000,
        AccelerometerFilteredAll = 0x0000E000,

        MotorPWM =  0x00100000 | 0x00080000, // -2048 -> 2047

        Magnetometer = 0x02000000,

        GyroZRaw = 0x04000000,
        GyroYRaw = 0x08000000,
        GyroXRaw = 0x10000000,

        AccelerometerZRaw = 0x20000000,
        AccelerometerYRaw = 0x40000000,
        AccelerometerXRaw = 0x80000000,
        AccelerometerRaw =  0xE0000000,

        AllSources = 0xFFFFFFFF,
    };

    uint16_t maxRateDivisor = 10;
    uint16_t framesPerPacket = 1;
    uint32_t sourceMask = AllSources;
    uint8_t packetCount = 0; // 0 == forever

    static QByteArray create(const int packetCount, const uint16_t maxRateDivisor = 10, const uint16_t framesPerPacket = 1, const uint32_t sourceMask = AllSources) {
        DataStreamingCommandPacket def;
        def.packetCount = packetCount;
        def.maxRateDivisor = maxRateDivisor;
        def.framesPerPacket = framesPerPacket;
        def.sourceMask = sourceMask;
        return packetToByteArray(def);
    }
};

struct DataStreamingCommandPacket1_17 : DataStreamingCommandPacket
{
    enum SourceMask : uint64_t {
        Quaternion0 = 0x80000000,
        Quaternion1 = 0x40000000,
        Quaternion2 = 0x20000000,
        Quaternion3 = 0x10000000,
        LocatorX = 0x0800000,
        LocatorY = 0x0400000,

        // TODO:
        //MaskAccelOne                  = 0x0200000000000000,

        VelocityX = 0x0100000,
        VelocityY = 0x0080000,

        LocatorAll = 0x0D80000,
        QuaternionAll = 0xF0000000,

        AllSourcesHigh = 0xFFFFFFFF,

    };
    uint32_t sourceMaskHighBits = AllSourcesHigh; // firmware >= 1.17
};

struct SetColorsCommandPacket
{
    static constexpr uint32_t deviceId = CommandPacketHeader::HardwareControl;
    static constexpr uint32_t commandId = CommandPacketHeader::SetRGBLed;

    enum Assignment {
        Temporary,
        Permanent
    };

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t setAsDefault = 0;

    SetColorsCommandPacket(const uint8_t red, const uint8_t green, const uint8_t blue, const Assignment assignment) :
        r(red),
        g(green),
        b(blue),
        setAsDefault(assignment == Permanent ? 1 : 0)
    { }
};

struct RollCommandPacket
{
    static constexpr uint32_t deviceId = CommandPacketHeader::HardwareControl;
    static constexpr uint32_t commandId = CommandPacketHeader::Roll;

    enum Type : uint8_t {
        Brake = 0,
        Roll = 1,
        Calibrate = 2
    };

    uint8_t speed = 0;
    uint16_t angle = 0;
    uint8_t type = Roll;
};

struct SetHeadingPacket
{
    static constexpr uint32_t deviceId = CommandPacketHeader::HardwareControl;
    static constexpr uint32_t commandId = CommandPacketHeader::SetHeading;

    uint16_t heading = 0;
};

struct EnableCollisionDetectionPacket
{
    static constexpr uint32_t deviceId = CommandPacketHeader::HardwareControl;
    static constexpr uint32_t commandId = CommandPacketHeader::ConfigureCollisionDetection;

    // The official SDK calls this "method", but also says that only method `1` is supported for now.
    uint8_t enabled = 1;

    uint8_t thresholdX = 100;
    uint8_t scaledThresholdX = 1; // This gets scaled/multiplied(?) by the speed and added to the normal threshold
    uint8_t thresholdY = 100;
    uint8_t scaledThresholdY = 1; // This gets scaled/multiplied(?) by the speed and added to the normal threshold
    uint8_t delay = 10; // Time (in seconds) from a collision is reported until detection starts again

    EnableCollisionDetectionPacket(const bool enabled_, const Vector2D<uint8_t> threshold_ = {100, 100}, const Vector2D<uint8_t> scaledThreshold_ = {1, 1}) {
        this->enabled = enabled_ ? 1 : 0;
        this->thresholdX = threshold_.x;
        this->thresholdY = threshold_.y;
        this->scaledThresholdX = scaledThreshold_.x;
        this->scaledThresholdY = scaledThreshold_.y;
    }
};

struct GoToSleepPacket
{
    static constexpr uint32_t deviceId = CommandPacketHeader::Internal;
    static constexpr uint32_t commandId = CommandPacketHeader::Sleep;

    // IDK where this is used, but it is in the official SDK
    // I thought this was controlled by the separate power bluetooth characteristic
    enum SleepType : uint8_t {
        NormalSleep = 0, // Light sleep, keeps high report rate for bluetooth
        DeepSleep = 1, // idk
        LowPowerSleep = 2 // idk, I guess lower slepe
    };

    uint16_t wakeupInterval = 5; // Time (in seconds) of intervals between automatically waking, if 0 sleeps forever
    uint8_t wakeMacro = 0; // If >0 macro to run when waking
    uint16_t wakeScriptLineNumber = 0; // If >0 the line number of the script in flash to run when waking

    GoToSleepPacket(const uint16_t wakeInterval_ = 5) : wakeupInterval(wakeInterval_) {
    }
};

struct SetNonPersistentOptionsPacket
{
    enum Options {
        StopOnDisconnect = 1, // Force stop when disconnected
        CompatibilityMode = 2 // Some compatibility mode? only for Ollie, says the doc
    };

    uint32_t optionsBitmask;

    static QByteArray create(const uint32_t options = StopOnDisconnect) {
        SetNonPersistentOptionsPacket def;
        def.optionsBitmask = options;
        return packetToByteArray(def);
    }
};

// Disables stabilization and moves full power in the requested direction
struct BoostCommandPacket
{
    static constexpr uint32_t deviceId = CommandPacketHeader::HardwareControl;
    static constexpr uint32_t commandId = CommandPacketHeader::Boost;

    uint8_t duration = 1; // tenths of seconds, 0 means forever (until stabilization command is received)
    uint16_t direction = 0; // in degrees, 0-360
};

// I think this is the SetDeviceMode
struct SetUserHackModePacket
{
    static constexpr uint32_t deviceId = CommandPacketHeader::HardwareControl;
    static constexpr uint32_t commandId = CommandPacketHeader::SetDeviceMode;

    // Enables ascii shell commands?
    uint8_t enabled = 0;
};

static_assert(sizeof(CommandPacketHeader) == 6);

#pragma pack(pop)

} // namespace sphero
