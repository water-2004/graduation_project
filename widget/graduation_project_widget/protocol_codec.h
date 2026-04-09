#ifndef GP_QT_PROTOCOL_CODEC_H_
#define GP_QT_PROTOCOL_CODEC_H_

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include "protocol/message_protocol.h"

namespace gp::qt_protocol {

enum class PacketDecodeStatus {
    kNeedMoreData,
    kSuccess,
    kError,
};

QByteArray EncodeJsonPacket(
    gp::protocol::MessageType type,
    const QJsonObject& payload = QJsonObject());
PacketDecodeStatus TryTakePacket(
    QByteArray* buffer,
    gp::protocol::Packet* packet,
    QString* error_text);
bool ParseJsonObject(
    const gp::protocol::Packet& packet,
    QJsonObject* object,
    QString* error_text);
QString MessageTypeToText(gp::protocol::MessageType type);

}  // namespace gp::qt_protocol

#endif  // GP_QT_PROTOCOL_CODEC_H_
