#include <QMetaEnum>
#include <QDebug>

#include "SpheroHandler.h"
#include "utils.h"
#include "Uuids.h"

#include <QLowEnergyController>
#include <QLowEnergyConnectionParameters>
#include <QTimer>
#include <QDateTime>
#include <QtEndian>

namespace sphero {

SpheroHandler::SpheroHandler(const QBluetoothDeviceInfo &deviceInfo, QObject *parent) :
    QObject(parent),
    m_name(deviceInfo.name())
{
    if (m_name.startsWith("BB-8")) {
        m_robotType = SpheroType::Bb8;
    }

    qDebug() << sizeof(SensorStreamPacket);
    m_deviceController = QLowEnergyController::createCentral(deviceInfo, this);

    connect(m_deviceController, &QLowEnergyController::connected, m_deviceController, &QLowEnergyController::discoverServices);
    connect(m_deviceController, &QLowEnergyController::discoveryFinished, this, &SpheroHandler::onServiceDiscoveryFinished);

    connect(m_deviceController, &QLowEnergyController::connectionUpdated, this, [](const QLowEnergyConnectionParameters &parms) {
            qDebug() << " - controller connection updated, latency" << parms.latency() << "maxinterval:" << parms.maximumInterval() << "mininterval:" << parms.minimumInterval() << "supervision timeout" << parms.supervisionTimeout();
            });
    connect(m_deviceController, &QLowEnergyController::connected, this, []() {
            qDebug() << " - controller connected";
            });
    connect(m_deviceController, &QLowEnergyController::disconnected, this, []() {
            qDebug() << " ! controller disconnected";
            });
    connect(m_deviceController, &QLowEnergyController::discoveryFinished, this, []() {
            qDebug() << " - controller discovery finished";
            });
    connect(m_deviceController, QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::error), this, &SpheroHandler::onControllerError);

    connect(m_deviceController, &QLowEnergyController::stateChanged, this, &SpheroHandler::onControllerStateChanged);

    m_deviceController->connectToDevice();

    if (m_deviceController->error() != QLowEnergyController::NoError) {
        qWarning() << " ! controller error when starting:" << m_deviceController->error() << m_deviceController->errorString();
    }
    qDebug() << " - Created handler";
}

SpheroHandler::~SpheroHandler()
{
    qDebug() << " - sphero handler dead";
    if (m_deviceController) {
        m_deviceController->disconnectFromDevice();
    } else {
        qWarning() << "no controller";
    }
}

bool SpheroHandler::isConnected()
{
    return m_deviceController && m_deviceController->state() != QLowEnergyController::UnconnectedState &&
            m_mainService && m_mainService->state() == QLowEnergyService::ServiceDiscovered &&
            m_radioService && m_radioService->state() == QLowEnergyService::ServiceDiscovered &&
            m_commandsCharacteristic.isValid();
}

QString SpheroHandler::statusString()
{
    const QString name = m_name.isEmpty() ? "device" : m_name;
    if (isConnected()) {
        return tr("Connected to %1").arg(name);
    } else if (m_deviceController && m_deviceController->state() == QLowEnergyController::UnconnectedState) {
//        QTimer::singleShot(1000, this, &QObject::deleteLater);
        return tr("Failed to connect to %1").arg(name);
    } else {
        return tr("Found %1, trying to establish connection...").arg(name);
    }
}

void SpheroHandler::onServiceDiscoveryFinished()
{
    qDebug() << " - Discovered services";


    m_radioService = m_deviceController->createServiceObject(Services::radio, this);
    if (!m_radioService) {
        qWarning() << " ! Failed to get ble service";
        return;
    }
    qDebug() << " - Got ble service";

    connect(m_radioService, &QLowEnergyService::characteristicChanged, this, &SpheroHandler::onCharacteristicChanged);
    connect(m_radioService, &QLowEnergyService::stateChanged, this, &SpheroHandler::onRadioServiceChanged);
    connect(m_radioService, QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::error), this, &SpheroHandler::onServiceError);

    connect(m_radioService, &QLowEnergyService::characteristicWritten, this, [](const QLowEnergyCharacteristic &c, const QByteArray &v) {
        qDebug() << " - " << c.uuid() << "radio written" << v;
    });


    if (m_mainService) {
        qWarning() << " ! main service already exists!";
        return;
    }

    m_mainService = m_deviceController->createServiceObject(Services::main, this);
    if (!m_mainService) {
        qWarning() << " ! no main service";
        return;
    }
    connect(m_mainService, &QLowEnergyService::characteristicChanged, this, &SpheroHandler::onCharacteristicChanged);
    connect(m_mainService, QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::error), this, &SpheroHandler::onServiceError);
    connect(m_mainService, &QLowEnergyService::characteristicWritten, this, [](const QLowEnergyCharacteristic &info, const QByteArray &value) {
        qDebug() << " - main written" << info.uuid() << value.toHex(':');
    });
    connect(m_mainService, &QLowEnergyService::stateChanged, this, &SpheroHandler::onMainServiceChanged);

    m_radioService->discoverDetails();
}

void SpheroHandler::onMainServiceChanged(QLowEnergyService::ServiceState newState)
{
    qDebug() << " ! mainservice change" << newState;

    if (newState == QLowEnergyService::InvalidService) {
        qWarning() << "Got invalid service";
        emit disconnected();
        return;
    }

    if (newState == QLowEnergyService::DiscoveringServices) {
        return;
    }

    if (newState != QLowEnergyService::ServiceDiscovered) {
        qDebug() << " ! unhandled service state changed:" << newState;
        return;
    }

    m_commandsCharacteristic = m_mainService->characteristic(Characteristics::commands);
    if (!m_commandsCharacteristic.isValid()) {
        qWarning() << "Commands characteristic invalid";
        return;
    }

    const QLowEnergyCharacteristic responseCharacteristic = m_mainService->characteristic(Characteristics::response);
    if (!responseCharacteristic.isValid()) {
        qWarning() << "response characteristic invalid";
        return;
    }

    // Enable notifications
    m_mainService->writeDescriptor(responseCharacteristic.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration), QByteArray::fromHex("0100"));

    qDebug() << " - Successfully connected";

    sendCommand(PacketHeader::HardwareControl, PacketHeader::GetLocatorData, "", PacketHeader::Synchronous, PacketHeader::ResetTimeout);

    emit connectedChanged();
}

void SpheroHandler::onControllerStateChanged(QLowEnergyController::ControllerState state)
{
    if (state == QLowEnergyController::UnconnectedState) {
        qWarning() << " ! Disconnected";
        emit disconnected();
        return;
    }

    qDebug() << " - controller state changed" << state;
    emit connectedChanged();
}

void SpheroHandler::onControllerError(QLowEnergyController::Error newError)
{
    qWarning() << " - controller error:" << newError << m_deviceController->errorString();
    if (newError == QLowEnergyController::UnknownError) {
        qWarning() << "Probably 'Operation already in progress' because qtbluetooth doesn't understand why it can't get answers over dbus when a connection attempt hangs";
    }
    emit disconnected();
}


void SpheroHandler::onServiceError(QLowEnergyService::ServiceError error)
{
    qWarning() << "Service error:" << error;
    if (error == QLowEnergyService::NoError) {
        return;
    }

    emit disconnected();
}

void SpheroHandler::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &data)
{
    if (data.isEmpty()) {
        qWarning() << " ! " << characteristic.uuid() << "got empty data";
        return;
    }

    if (characteristic.uuid() == Characteristics::Radio::rssi) {
        m_rssi = data[0];
        emit rssiChanged();
        return;
    }
    if (characteristic.uuid() == QBluetoothUuid::ServiceChanged) {
        qDebug() << " ? GATT service changed" << data.toHex(':');
        return;
    }

    if (characteristic.uuid() != Characteristics::response) {
        qWarning() << " ? Changed from unexpected characteristic" << characteristic.name() << characteristic.uuid() << data;
        return;
    }

    qDebug() << " ------------ Characteristic changed" << data.toHex(':');

    if (data.isEmpty()) {
        qWarning() << " ! No data received";
        return;
    }

    // New messages start with 0xFF, so reset buffer in that case
    if (data.startsWith(0xFF)) {
        m_receiveBuffer = data;
    } else if (data.startsWith("u>\xff\xff")) {
        // We don't always get this
         // I _think_ the 'u>' is a separate packet, so just strip it
        qDebug() << " - Got unknown something that looks like a prompt (u>), is an ack of some sorts?";
        m_receiveBuffer = data.mid(2);
    } else if (!m_receiveBuffer.isEmpty()) {
        m_receiveBuffer.append(data);
    } else {
        qWarning() << " ! Got data but without correct start";
        qDebug() << data;
        const int startOfData = data.indexOf(0xFF);
        if (startOfData < 0) {
            qWarning() << " ! Contains nothing useful";
            m_receiveBuffer.clear();
            return;
        }
        m_receiveBuffer = data.mid(startOfData);
    }

    if (m_receiveBuffer.size() > 10000) {
        qWarning() << " ! Receive buffer too large, nuking" << m_receiveBuffer.size();
        m_receiveBuffer.clear();
        return;
    }

    if (m_receiveBuffer.size() < int(sizeof(PacketHeader))) {
        qDebug() << " - Not a full header" << m_receiveBuffer.size();
        return;
    }

    ResponsePacketHeader header;
    if (header.response != ResponsePacketHeader::Ok) {

    }
    qFromBigEndian<uint8_t>(m_receiveBuffer.data(), sizeof(PacketHeader), &header);
    qDebug() << " - magic" << header.magic;
    if (header.magic != 0xFF) {
        qWarning() << " ! Invalid magic";
        return;
    }
    qDebug() << " - type" << header.type;
    switch(header.type) {
    case ResponsePacketHeader::Ack:
        qDebug() << " - ack response" << ResponsePacketHeader::AckType(header.response);
        break;
    case ResponsePacketHeader::Data:
        qDebug() << " - data response" << ResponsePacketHeader::DataType(header.response);
        break;
    default:
        qWarning() << " ! unhandled type" << header.type;
        m_receiveBuffer.clear();
    }

    qDebug() << " - sequence num" << header.sequenceNumber;
    qDebug() << " - data length" << header.dataLength;

    if (m_receiveBuffer.size() != sizeof(PacketHeader) + header.dataLength - 1) {
        qWarning() << " ! Packet size wrong" << m_receiveBuffer.size();
        qDebug() << "  > Expected" << sizeof(PacketHeader) << "+" << header.dataLength;
        return;
    }

    QByteArray contents;

    uint8_t checksum = 0;
    for (int i=2; i<m_receiveBuffer.size() - 1; i++) {
        checksum += uint8_t(m_receiveBuffer[i]);
        contents.append(m_receiveBuffer[i]);
    }
    checksum ^= 0xFF;
    if (!m_receiveBuffer.endsWith(checksum)) {
        qWarning() << " ! Invalid checksum" << checksum;
        qDebug() << "  > Expected" << uint8_t(m_receiveBuffer.back());
        return;
    }
    qDebug() << " - received contents" << contents.size() << contents.toHex(':');
    contents = contents.left(header.dataLength);

    qDebug() << " - response type:" << header.type;

    switch(header.type) {
    case ResponsePacketHeader::Ack: {
        qDebug() << " - ack response" << ResponsePacketHeader::AckType(header.response);
        qDebug() << "Content length" << contents.length() << "data length" << header.dataLength << "buffer length" << m_receiveBuffer.length() << "locator packet size" << sizeof(LocatorPacket) << "response packet size" << sizeof(ResponsePacketHeader);

        // TODO separate function
        if (size_t(contents.size()) < sizeof(AckResponsePacket)) {
            qWarning() << "Impossibly short data response packet, size" << contents.size() << "we require at least" << sizeof(AckResponsePacket);
            qDebug() << contents;
            return;
        }
        const AckResponsePacket *response = reinterpret_cast<const AckResponsePacket*>(contents.data());
        qDebug() << "Response type" << response->type << "unknown" << response->unk;
        switch(response->type) {
        case LocatorResponse: {
            if (size_t(contents.length()) < sizeof(LocatorPacket)) {
                qWarning() << "Locator response too small" << contents.length();
                return;
            }
            contents = contents.mid(4);
            qDebug() << "Locator size" << sizeof(LocatorPacket) << "Locatorconf" << contents.size();
            const LocatorPacket *location = reinterpret_cast<const LocatorPacket*>(contents.data());
            qDebug() << "tilt" << location->tilt << "position" << location->position.x << location->position.y << (location->flags ? "calibrated" : "not calibrated");

            DataStreamingCommandPacket def;
            def.packetCount = 1;
            sendCommand(PacketHeader::HardwareControl, PacketHeader::SetDataStreaming, QByteArray((char*)&def, sizeof(def)), PacketHeader::Synchronous, PacketHeader::ResetTimeout);
            break;
        }
        default:
            qWarning() << "Unhandled ack response" << ResponseType(response->type);
            break;
        }

        break;
    }
    case ResponsePacketHeader::Data:
        qDebug() << " - data response" << ResponsePacketHeader::DataType(header.response);
        break;
    default:
        qWarning() << " ! unhandled type" << header.type;
        m_receiveBuffer.clear();
    }

    if (header.type != ResponsePacketHeader::Ack) {
        qWarning() << " ! not a simple response";
        return;
    }

//    // async
//    switch(header.response) {
//    case StreamingResponse:
//        if (m_receiveBuffer.size() != 88) {
//            qWarning() << "Invalid streaming data size" << m_receiveBuffer.size();
//            break;
//        }
//        qDebug() << "Got streaming command";
//        break;
//    case PowerStateResponse: {
//        if (m_receiveBuffer.size() != sizeof(PowerStatePacket)) {
//            qWarning() << "Invalid size of powerstate packet" << m_receiveBuffer.size();
//            qDebug() << "Expected" << sizeof(PowerStatePacket);
//            break;
//        }

//        qDebug() << "Got powerstate response";
//        PowerStatePacket powerState;
//        qFromBigEndian<uint8_t>(m_receiveBuffer.data(), sizeof(PowerStatePacket), &powerState);
//        qDebug() << "record version" << powerState.recordVersion;
//        qDebug() << "power state" << powerState.powerState;
//        qDebug() << "battery voltage" << powerState.batteryVoltage;
//        qDebug() << "number of charges" << powerState.numberOfCharges;
//        qDebug() << "seconds since last charge" << powerState.secondsSinceCharge;

//        break;
//    }
//    case LocatorResponse: {
//        if (m_receiveBuffer.size() != 16) {
//            qWarning() << "Invalid size of locator response" << m_receiveBuffer.size();
//            break;
//        }
//        qDebug() << "Got locator response";
//        LocatorPacket locator;
//        qFromBigEndian<uint8_t>(m_receiveBuffer.data(), sizeof(PowerStatePacket), &locator);
//        qDebug() << "flags" << locator.flags;
//        qDebug() << "X:" << locator.position.x;
//        qDebug() << "Y:" << locator.position.y;
//        qDebug() << "tilt:" << locator.tilt;

//        break;
//    }
//    default:
//        qDebug() << "Unknown command" << header.commandID;
//        break;
//    }

//    switch(m_receiveBuffer[2]) {

    //    }
}

void SpheroHandler::onRadioServiceChanged(QLowEnergyService::ServiceState newState)
{
    if (newState == QLowEnergyService::DiscoveringServices) {
        return;
    }
    if (newState != QLowEnergyService::ServiceDiscovered) {
        qDebug() << " ! unhandled radio service state changed:" << newState;
        return;
    }

    if (!sendRadioControlCommand(Characteristics::Radio::antiDos, "011i3") ||
        !sendRadioControlCommand(Characteristics::Radio::transmitPower, QByteArray(1, 7)) ||
        !sendRadioControlCommand(Characteristics::Radio::wake, QByteArray(1, 1))) {
        qWarning() << " ! Init sequence failed";
        emit disconnected();
        return;
    }
    const QLowEnergyCharacteristic rssiCharacteristic = m_radioService->characteristic(Characteristics::Radio::rssi);
    m_radioService->writeDescriptor(rssiCharacteristic.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration), QByteArray::fromHex("0100"));

    qDebug() << " - Init sequence done";
    m_mainService->discoverDetails();
}

bool SpheroHandler::sendRadioControlCommand(const QBluetoothUuid &characteristicUuid, const QByteArray &data)
{
    if (!m_radioService || m_radioService->state() != QLowEnergyService::ServiceDiscovered) {
        qWarning() << "Radio service not connected";
        return false;
    }
    QLowEnergyCharacteristic characteristic = m_radioService->characteristic(characteristicUuid);
    if (!characteristic.isValid()) {
        qWarning() << "Radio characteristic" << characteristicUuid << "not available";
        return false;
    }
    m_radioService->writeCharacteristic(characteristic, data);
    return true;
}

void SpheroHandler::sendCommand(const uint8_t deviceId, const uint8_t commandID, const QByteArray &data, const PacketHeader::SynchronousType synchronous, const PacketHeader::TimeoutHandling keepTimeout)
{
    static int currentSequenceNumber = 0;
    PacketHeader header;
    header.dataLength = data.size() + 1; // + 1 for checksum
    header.flags |= synchronous;
    header.flags |= keepTimeout;
//    header.synchronous = synchronous;
//    header.resetTimeout = keepTimeout;
    header.sequenceNumber = currentSequenceNumber++;
    header.commandID = commandID;
    header.deviceID = deviceId;
    qDebug() << " + Packet:";
    qDebug() << "  ] Device id:" << header.deviceID;
    qDebug() << "  ] command id:" << header.commandID;
    qDebug() << "  ] seq number:" << header.sequenceNumber;

    if (header.dataLength != data.size() + 1) {
        qWarning() << "Invalid data length in header";
    }
    QByteArray headerBuffer(sizeof(PacketHeader), 0); // + 1 for checksum
    qToBigEndian<uint8_t>(&header, sizeof(PacketHeader), headerBuffer.data());
    qDebug() << " - " << uchar(headerBuffer[0]);
    qDebug() << " - " << uchar(headerBuffer[1]);

    QByteArray toSend;
    toSend.append(headerBuffer);
    toSend.append(data);

    uint8_t checksum = 0;
    for (int i=2; i<toSend.size(); i++) {
        checksum += toSend[i];
    }
    toSend.append(checksum xor 0xFF);

//    qDebug() << " - Writing command" << toSend.toHex();
    m_mainService->writeCharacteristic(m_commandsCharacteristic, toSend);
}

} // namespace sphero
