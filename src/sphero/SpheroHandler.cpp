#include <QMetaEnum>
#include <QDebug>

#include "SpheroHandler.h"
#include "utils.h"

#include <QLowEnergyController>
#include <QLowEnergyConnectionParameters>
#include <QTimer>
#include <QDateTime>
#include <QtEndian>

namespace sphero {

namespace Characteristics {
namespace Radio {
    static const QBluetoothUuid unknownRadio(QStringLiteral("{22BB746F-2BB1-7554-2D6F-726568705327}"));
    static const QBluetoothUuid transmitPower(QStringLiteral("{22BB746F-2BB2-7554-2D6F-726568705327}"));
    static const QBluetoothUuid rssi(QStringLiteral("{22BB746F-2BB6-7554-2D6F-726568705327}"));
    static const QBluetoothUuid deepSleep(QStringLiteral("{22BB746F-2BB7-7554-2D6F-726568705327}"));
    static const QBluetoothUuid unknownRadio2(QStringLiteral("{22BB746F-2BB8-7554-2D6F-726568705327}"));
    static const QBluetoothUuid unknownRadio3(QStringLiteral("{22BB746F-2BB9-7554-2D6F-726568705327}"));
    static const QBluetoothUuid unknownRadio4(QStringLiteral("{22BB746F-2BBA-7554-2D6F-726568705327}"));
    static const QBluetoothUuid antiDos(QStringLiteral("{22BB746F-2BBD-7554-2D6F-726568705327}"));
    static const QBluetoothUuid antiDosTimeout(QStringLiteral("{22BB746F-2BBE-7554-2D6F-726568705327}"));
    static const QBluetoothUuid wake(QStringLiteral("{22BB746F-2BBF-7554-2D6F-726568705327}"));
    static const QBluetoothUuid unknownRadio5(QStringLiteral("{22BB746F-3BBA-7554-2D6F-726568705327}"));
}; // namespace Radio

} // namespace Characteristics

SpheroHandler::SpheroHandler(const QBluetoothDeviceInfo &deviceInfo, QObject *parent) :
    QObject(parent),
    m_name(deviceInfo.name())
{
    m_deviceController = QLowEnergyController::createCentral(deviceInfo, this);

    connect(m_deviceController, &QLowEnergyController::connected, m_deviceController, &QLowEnergyController::discoverServices);
    connect(m_deviceController, &QLowEnergyController::serviceDiscovered, this, &SpheroHandler::onServiceDiscovered);

    connect(m_deviceController, &QLowEnergyController::connectionUpdated, this, [](const QLowEnergyConnectionParameters &parms) {
            qDebug() << " - controller connection updated, latency" << parms.latency() << "maxinterval:" << parms.maximumInterval() << "mininterval:" << parms.minimumInterval() << "supervision timeout" << parms.supervisionTimeout();
            });
    connect(m_deviceController, &QLowEnergyController::connected, this, []() {
            qDebug() << " - controller connected";
            });
    connect(m_deviceController, &QLowEnergyController::disconnected, this, []() {
            qDebug() << " - controller disconnected";
            });
    connect(m_deviceController, &QLowEnergyController::discoveryFinished, this, []() {
            qDebug() << " - controller discovery finished";
            });
    connect(m_deviceController, QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::error), this, &SpheroHandler::onControllerError);

    connect(m_deviceController, &QLowEnergyController::stateChanged, this, &SpheroHandler::onControllerStateChanged);

    m_deviceController->connectToDevice();

    if (m_deviceController->error() != QLowEnergyController::NoError) {
        qDebug() << "controller error when starting:" << m_deviceController->error() << m_deviceController->errorString();
    }
    qDebug() << "Created handler";
}

SpheroHandler::~SpheroHandler()
{
    qDebug() << "sphero handler dead";
    if (m_deviceController) {
        m_deviceController->disconnectFromDevice();
    } else {
        qWarning() << "no controller";
    }
}

bool SpheroHandler::isConnected()
{
    return m_deviceController && m_deviceController->state() != QLowEnergyController::UnconnectedState && m_mainService &&
//            m_spheroBLECharacteristic.isValid() &&
//            m_robotControlCharacteristic.isValid() &&
            m_rssiCharacteristic.isValid() &&
            m_wakeCharacteristic.isValid() &&
            m_txPowerCharacteristic.isValid() &&
            m_commandsCharacteristic.isValid() &&
            m_responseCharacteristic.isValid();
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

void SpheroHandler::onServiceDiscovered(const QBluetoothUuid &newService)
{
    qDebug() << "Discovered service" << newService;

    static const QBluetoothUuid fwUpdateServiceLM  =  QUuid(0x00010001, 0x574f, 0x4f20, 0x53, 0x70, 0x68, 0x65, 0x72, 0x6f, 0x21, 0x21);

    static const QBluetoothUuid mainService(QStringLiteral("{22bb746f-2ba0-7554-2d6f-726568705327}"));
    static const QBluetoothUuid spheroBLEService(QStringLiteral("{22bb746f-2bb0-7554-2d6f-726568705327}"));
    static const QBluetoothUuid batteryControlService(QStringLiteral("{00001016-d102-11e1-9b23-00025b00a5a5}"));

    QString serviceType;
    // could use switch/case on uint32, but don't know how unique that is
    if (newService == mainService) {
        qDebug() << " gotmain service";
        return;
    } else if (newService == spheroBLEService) {
        qDebug() << "Got ble service";
        QPointer<QLowEnergyService> service = m_deviceController->createServiceObject(newService, this);
        m_bleService = service;

        connect(service, &QLowEnergyService::characteristicChanged, this, &SpheroHandler::onCharacteristicChanged);
        connect(service, &QLowEnergyService::stateChanged, this, &SpheroHandler::onBleServiceChanged);
        connect(service, &QLowEnergyService::characteristicWritten, this, &SpheroHandler::onBleCharacteristicWritten);

        service->discoverDetails();

        return;
    }
    return;

    QPointer<QLowEnergyService> service = m_deviceController->createServiceObject(newService, this);

    if (newService == batteryControlService) {
        serviceType = "battery control";
        qDebug() << "battery control service" << service->type();
//        return;
    } else if (newService == fwUpdateServiceLM) {
        serviceType = "fw update lm";
        qDebug() << "fw update service lm";
    } else if (newService == QBluetoothUuid::DeviceInformation) {
        serviceType = "device info";
        qDebug() << service->type();
        qDebug() << "device info service";
//        service->deleteLater();
//        return;
    } else if (newService == QBluetoothUuid::GenericAttribute) {
        serviceType = "generic attribute";
        qDebug() << "generic attribute service";
    } else {
        qWarning() << "Unknown service" << newService;
//        service->deleteLater();
//        return;
    }


    qDebug() << "got service:"  << service->serviceName() << service->serviceUuid();

    connect(service, &QLowEnergyService::characteristicChanged, this, &SpheroHandler::onCharacteristicChanged);
    connect(service, &QLowEnergyService::characteristicRead, this, &SpheroHandler::onCharacteristicRead);
    //connect(m_service, &QLowEnergyService::characteristicWritten, this, [](const QLowEnergyCharacteristic &descriptor, const QByteArray &newValue) {
    //        qDebug() << "Characteristic" << descriptor.uuid() << "wrote" << newValue;
    //        });;

    connect(service, &QLowEnergyService::descriptorRead, this, &SpheroHandler::onDescriptorRead);
    //connect(m_service, &QLowEnergyService::descriptorWritten, this, [](const QLowEnergyDescriptor &descriptor, const QByteArray &newValue) {
    //        qDebug() << "Descriptor" << descriptor.uuid() << "wrote" << newValue;
    //        });;

    connect(service, QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::error), this, &SpheroHandler::onServiceError);
    connect(service, &QLowEnergyService::stateChanged, this, [service, serviceType](QLowEnergyService::ServiceState newState) {
        if (newState == QLowEnergyService::InvalidService) {
            qWarning() << "Got invalid service";
            return;
        }

        if (newState == QLowEnergyService::DiscoveringServices) {
            qDebug() << "Discovering services";
            return;
        }


        if (newState != QLowEnergyService::ServiceDiscovered) {
            qDebug() << "unhandled service state changed:" << newState;
            return;
        }

//        qDebug() <<
        for (const QLowEnergyCharacteristic &c : service->characteristics()) {
            qDebug() << serviceType << "characteristic available:" << c.name() << c.uuid() << c.properties();
        }

    });

    service->discoverDetails();
}

void SpheroHandler::onMainServiceChanged(QLowEnergyService::ServiceState newState)
{
    static const QBluetoothUuid commandsUuid(QStringLiteral("{22BB746F-2BA1-7554-2D6F-726568705327}"));
    static const QBluetoothUuid responseUuid(QStringLiteral("{22BB746F-2BA6-7554-2D6F-726568705327}"));

    if (newState == QLowEnergyService::InvalidService) {
        qWarning() << "Got invalid service";
        emit disconnected();
        return;
    }

    if (newState == QLowEnergyService::DiscoveringServices) {
        qDebug() << "Discovering services";
        return;
    }


    if (newState != QLowEnergyService::ServiceDiscovered) {
        qDebug() << "unhandled service state changed:" << newState;
        return;
    }

    m_commandsCharacteristic = m_mainService->characteristic(commandsUuid);
    if (!m_commandsCharacteristic.isValid()) {
        qWarning() << "Commands characteristic invalid";
        return;
    }
    m_responseCharacteristic = m_mainService->characteristic(responseUuid);
    if (!m_responseCharacteristic.isValid()) {
        qWarning() << "response characteristic invalid";
        return;
    }

    for (const QLowEnergyCharacteristic &c : m_mainService->characteristics()) {
        qDebug() << "characteristic available:" << c.name() << c.uuid() << c.properties();
    }

    if (!m_responseCharacteristic.descriptors().isEmpty()) {
        m_readDescriptor = m_responseCharacteristic.descriptors().first();
    } else {
        qWarning() << "No response descriptors";
    }

//    if (!isConnected()) {
//        qDebug() << "Finished scanning, but not valid";
//        emit disconnected();
//        return;
//    }

    qDebug() << "Successfully connected";

//     Who the _fuck_ designed this API, requiring me to write magic bytes to a
//     fucking read descriptor to get characteristicChanged to work?
    qDebug() << "Writing to write descriptor";
    m_mainService->writeDescriptor(m_readDescriptor, QByteArray::fromHex("0100"));
    qDebug() << "writted to write descriptor";

//    qDebug() << "Requested notifications on values changes";


    m_mainService->readCharacteristic(m_responseCharacteristic);
    sendCommand(0x2, 0x15, "", PacketHeader::Synchronous, PacketHeader::KeepTimeout);
    emit connectedChanged();
}

void SpheroHandler::onControllerStateChanged(QLowEnergyController::ControllerState state)
{
    if (state == QLowEnergyController::UnconnectedState) {
        qWarning() << "Disconnected";
        emit disconnected();
    }

    qWarning() << " ! controller state changed" << state;
    emit connectedChanged();
}

void SpheroHandler::onControllerError(QLowEnergyController::Error newError)
{
    qWarning() << " - controller error:" << newError << m_deviceController->errorString();
    if (newError == QLowEnergyController::UnknownError) {
        qWarning() << "Probably 'Operation already in progress' because qtbluetooth doesn't understand why it can't get answers over dbus when a connection attempt hangs";
    }
    connect(m_deviceController, QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::error), this, &SpheroHandler::onControllerError);
}


void SpheroHandler::onServiceError(QLowEnergyService::ServiceError error)
{
    qWarning() << "Service error:" << error;
    if (error == QLowEnergyService::NoError) {
        return;
    }

    emit disconnected();
}

void SpheroHandler::onCharacteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &data)
{
    if (characteristic != m_responseCharacteristic) {
        qWarning() << "data from unexpected characteristic" << characteristic.uuid() << data;
        return;
    }
    qDebug() << "data from read characteristic" << data.toHex();
    qDebug() << "data type:" << data[0];

//    emit dataRead(data);
}

void SpheroHandler::onDescriptorRead(const QLowEnergyDescriptor &descriptor, const QByteArray &data)
{

    if (descriptor != m_readDescriptor) {
        qWarning() << "data from unexpected descriptor" << descriptor.uuid() << data;
        return;
    }
    if (data.isEmpty()) {
        qWarning() << "got empty data";
        return;
    }
    qDebug() << "data from read descriptor" << data;
    qDebug() << "data type:" << int(data[0]);

//    emit dataRead(data);
}

void SpheroHandler::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &data)
{

    if (data.isEmpty()) {
        qWarning() << characteristic.uuid() << "got empty data";
        return;
    }

    if (characteristic.uuid() == Characteristics::Radio::rssi) {
        m_rssi = data[0];
        qDebug() << "RSsi" << m_rssi;
        return;
    }
    if (characteristic.uuid() == QBluetoothUuid::ServiceChanged) {
        qDebug() << "GATT service changed" << data.toHex();
        return;
    }

    if (characteristic != m_responseCharacteristic) {
        qWarning() << "changed from unexpected characteristic" << characteristic.name() << characteristic.uuid() << data;
        return;
    }

    static_assert(sizeof(PacketHeader) == 6);
    static_assert(sizeof(LocatorPacket) == 15);

    qDebug() << "Characteristic changed" << data;

    if (data.isEmpty()) {
        qWarning() << "No data received";
        return;
    }
    // New messages start with 0xFF, so reset buffer in that case
    if (data.startsWith(0xFF)) {
        m_receiveBuffer = data;
    } else {
        m_receiveBuffer.append(data);
    }

    if (m_receiveBuffer.size() > 10000) {
        qWarning() << "Receive buffer too large, nuking" << m_receiveBuffer.size();
        m_receiveBuffer.clear();
        return;
    }

    if (m_receiveBuffer.size() < int(sizeof(PacketHeader))) {
        qDebug() << "Not a full header" << m_receiveBuffer.size();
        return;
    }

    PacketHeader header;
    qFromBigEndian<uint8_t>(m_receiveBuffer.data(), sizeof(PacketHeader), &header);
    if (m_receiveBuffer.size() != sizeof(PacketHeader) + header.dataLength) {
        qWarning() << "Packet size wrong" << m_receiveBuffer.size();
        qDebug() << "Expected" << (sizeof(PacketHeader) + header.dataLength);
    }

    uint8_t checksum = 0;
    for (int i=2; i<m_receiveBuffer.size() - 1; i++) {
        checksum += m_receiveBuffer[i];
    }
    checksum ^= 0xFF;
    if (!m_receiveBuffer.endsWith(checksum)) {
        qWarning() << "Invalid checksum" << checksum;
        qDebug() << "Expected" << m_receiveBuffer.back();
        return;
    }

    switch(header.commandID) {
    case StreamingResponse:
        if (m_receiveBuffer.size() != 88) {
            qWarning() << "Invalid streaming data size" << m_receiveBuffer.size();
            break;
        }
        qDebug() << "Got streaming command";
        break;
    case PowerStateResponse: {
        if (m_receiveBuffer.size() != sizeof(PowerStatePacket)) {
            qWarning() << "Invalid size of powerstate packet" << m_receiveBuffer.size();
            qDebug() << "Expected" << sizeof(PowerStatePacket);
            break;
        }

        qDebug() << "Got powerstate response";
        PowerStatePacket powerState;
        qFromBigEndian<uint8_t>(m_receiveBuffer.data(), sizeof(PowerStatePacket), &powerState);
        qDebug() << "record version" << powerState.recordVersion;
        qDebug() << "power state" << powerState.powerState;
        qDebug() << "battery voltage" << powerState.batteryVoltage;
        qDebug() << "number of charges" << powerState.numberOfCharges;
        qDebug() << "seconds since last charge" << powerState.secondsSinceCharge;

        break;
    }
    case LocatorResponse: {
        if (m_receiveBuffer.size() != 16) {
            qWarning() << "Invalid size of locator response" << m_receiveBuffer.size();
            break;
        }
        qDebug() << "Got locator response";
        LocatorPacket locator;
        qFromBigEndian<uint8_t>(m_receiveBuffer.data(), sizeof(PowerStatePacket), &locator);
        qDebug() << "flags" << locator.flags;
        qDebug() << "X:" << locator.position.x;
        qDebug() << "Y:" << locator.position.y;
        qDebug() << "tilt:" << locator.tilt;

        break;
    }
    default:
        qDebug() << "Unknown command" << header.commandID;
        break;
    }

//    switch(m_receiveBuffer[2]) {

    //    }
}

void SpheroHandler::onBleServiceChanged(QLowEnergyService::ServiceState newState)
{
    if (newState != QLowEnergyService::ServiceDiscovered) {
        qDebug() << "unhandled ble service state changed:" << newState;
        return;
    }
    static const QBluetoothUuid rssiUuid(QStringLiteral("{22bb746f-2bb6-7554-2d6f-726568705327}"));
    QLowEnergyCharacteristic antiDos = m_bleService->characteristic(Characteristics::Radio::antiDos);
    if (!antiDos.isValid()) {
        qWarning() << "Anti dos characteristic not available";
        return;
    }
    m_bleService->writeCharacteristic(antiDos, "011i3");
    qDebug() << "Antidos written";


}

void SpheroHandler::onBleCharacteristicWritten(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue)
{
    if (characteristic.uuid() != Characteristics::Radio::antiDos) {
        qWarning() << "BLE wrote invalid characteristic" << characteristic.name() << characteristic.uuid();
        return;
    }
    disconnect(m_bleService, &QLowEnergyService::characteristicWritten, this, &SpheroHandler::onBleCharacteristicWritten);

    if (m_mainService) {
        qWarning() << "main service already exists!";
        return;
    }
    qDebug() << "BLE written" << newValue;

    static const QBluetoothUuid mainService(QStringLiteral("{22bb746f-2ba0-7554-2d6f-726568705327}"));
    QPointer<QLowEnergyService> service = m_deviceController->createServiceObject(mainService, this);
    m_mainService = service;
    connect(service, &QLowEnergyService::characteristicChanged, this, &SpheroHandler::onCharacteristicChanged);
    connect(service, &QLowEnergyService::stateChanged, this, &SpheroHandler::onMainServiceChanged);

    if (m_mainService) {
        m_mainService->discoverDetails();
    } else {
        qWarning() << "no main service";
    }

}

void SpheroHandler::setTxPower(uint8_t power)
{
    if (!isConnected()) {
        qWarning() << "Trying to set transmit power, we're not connected";
        return;
    }
    m_mainService->writeCharacteristic(m_txPowerCharacteristic, QByteArray(1, power));
}

void SpheroHandler::sendCommand(const uint8_t deviceId, const uint8_t commandID, const QByteArray &data, const PacketHeader::SynchronousType synchronous, const PacketHeader::TimeoutHandling keepTimeout)
{
    static int currentSequenceNumber = 0;
    PacketHeader header;
    header.dataLength = data.size() + 1; // + 1 for checksum
    header.synchronous = synchronous;
    header.resetTimeout = keepTimeout;
    header.sequenceNumber = currentSequenceNumber++;
    header.commandID = commandID;
    header.deviceID = deviceId;

    if (header.dataLength != data.size() + 1) {
        qWarning() << "Invalid data length in header";
    }
    QByteArray headerBuffer(sizeof(PacketHeader), 0); // + 1 for checksum
    qToBigEndian<uint8_t>(&header, sizeof(PacketHeader), headerBuffer.data());

    QByteArray toSend;
    toSend.append(headerBuffer);
    toSend.append(data);

//    for (const char &b : data) {
//        checksum += uint8_t(b) & 0xFF;
//    }
    uint8_t checksum = 0;
    for (int i=2; i<toSend.size(); i++) {
        checksum += toSend[i];
    }
    toSend.append(checksum xor 0xFF);

    qDebug() << "Writing command";
    m_mainService->writeCharacteristic(m_commandsCharacteristic, toSend);
    qDebug() << "wrote command";
}

} // namespace sphero