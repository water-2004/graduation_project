#include "protocol_codec.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace gp::qt_protocol {

QByteArray EncodeJsonPacket(gp::protocol::MessageType type, const QJsonObject& payload) {
    const QByteArray payload_bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const gp::protocol::Packet packet{type, payload_bytes.toStdString()};
    const std::string bytes = gp::protocol::EncodePacket(packet);
    return QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size()));
}

PacketDecodeStatus TryTakePacket(QByteArray* buffer, gp::protocol::Packet* packet, QString* error_text) {
    if (buffer == nullptr || packet == nullptr) {
        if (error_text != nullptr) {
            *error_text = QStringLiteral("协议层内部错误：空指针参数");
        }
        return PacketDecodeStatus::kError;
    }

    if (buffer->size() < static_cast<int>(gp::protocol::kHeaderSize)) {
        return PacketDecodeStatus::kNeedMoreData;
    }

    gp::protocol::PacketHeader header;
    std::string err;
    if (!gp::protocol::DecodeHeader(
            reinterpret_cast<const std::uint8_t*>(buffer->constData()),
            gp::protocol::kHeaderSize,
            &header,
            &err)) {
        if (error_text != nullptr) {
            *error_text = QString::fromUtf8(err.c_str());
        }
        return PacketDecodeStatus::kError;
    }

    const qsizetype total_length = static_cast<qsizetype>(gp::protocol::kHeaderSize + header.payload_length);
    if (buffer->size() < total_length) {
        return PacketDecodeStatus::kNeedMoreData;
    }

    packet->message_type = header.message_type;
    packet->payload.assign(
        buffer->constData() + static_cast<int>(gp::protocol::kHeaderSize),
        static_cast<std::size_t>(header.payload_length));
    buffer->remove(0, total_length);
    return PacketDecodeStatus::kSuccess;
}

bool ParseJsonObject(const gp::protocol::Packet& packet, QJsonObject* object, QString* error_text) {
    if (object == nullptr) {
        if (error_text != nullptr) {
            *error_text = QStringLiteral("协议层内部错误：JSON 输出参数为空");
        }
        return false;
    }

    const QByteArray payload_bytes(packet.payload.data(), static_cast<qsizetype>(packet.payload.size()));
    if (payload_bytes.isEmpty()) {
        *object = QJsonObject();
        return true;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(payload_bytes, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        if (error_text != nullptr) {
            *error_text = QStringLiteral("JSON 解析失败：%1").arg(parse_error.errorString());
        }
        return false;
    }
    if (!document.isObject()) {
        if (error_text != nullptr) {
            *error_text = QStringLiteral("JSON 负载不是对象类型");
        }
        return false;
    }

    *object = document.object();
    return true;
}

QString MessageTypeToText(gp::protocol::MessageType type) {
    return QString::fromLatin1(gp::protocol::MessageTypeName(type));
}

}  // namespace gp::qt_protocol
