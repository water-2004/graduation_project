#ifndef GP_SHARED_PROTOCOL_MESSAGE_PROTOCOL_H_
#define GP_SHARED_PROTOCOL_MESSAGE_PROTOCOL_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace gp::protocol {

constexpr std::uint16_t kMagic = 0x4750;   // ASCII: 'G' 'P'
constexpr std::uint8_t kVersion = 0x01;
constexpr std::size_t kHeaderSize = 9;
constexpr std::uint32_t kMaxPayloadLength = 4 * 1024 * 1024;  // 4 MB

enum class MessageType : std::uint16_t {
    kPingRequest = 0x0001,
    kPingResponse = 0x8001,
    kQuitRequest = 0x0002,
    kByeResponse = 0x8002,
    kErrorResponse = 0x8FFF,

    kPredictRequest = 0x0101,
    kPredictResponse = 0x8101,
    kListSamplesRequest = 0x0102,
    kListSamplesResponse = 0x8102,
    kPlaySampleRequest = 0x0103,
    kPlaySampleResponse = 0x8103,

    kLoginRequest = 0x0201,
    kLoginResponse = 0x8201,
    kListPatientsRequest = 0x0202,
    kListPatientsResponse = 0x8202,
    kGetPatientRequest = 0x0203,
    kGetPatientResponse = 0x8203,
    kAddPatientRequest = 0x0204,
    kAddPatientResponse = 0x8204,
    kUpdatePatientRequest = 0x0205,
    kUpdatePatientResponse = 0x8205,
    kDeletePatientRequest = 0x0206,
    kDeletePatientResponse = 0x8206,
    kAddMonitorRecordRequest = 0x0207,
    kAddMonitorRecordResponse = 0x8207,
    kListMonitorRecordsRequest = 0x0208,
    kListMonitorRecordsResponse = 0x8208,
    kListAllMonitorRecordsRequest = 0x0209,
    kListAllMonitorRecordsResponse = 0x8209,
    kListAlertsRequest = 0x020A,
    kListAlertsResponse = 0x820A,
    kListAllAlertsRequest = 0x020B,
    kListAllAlertsResponse = 0x820B,
    kConfirmAlertRequest = 0x020C,
    kConfirmAlertResponse = 0x820C,
    kChangePasswordRequest = 0x020D,
    kChangePasswordResponse = 0x820D,
};

struct PacketHeader {
    std::uint16_t magic = kMagic;
    std::uint8_t version = kVersion;
    MessageType message_type = MessageType::kErrorResponse;
    std::uint32_t payload_length = 0;
};

struct Packet {
    MessageType message_type = MessageType::kErrorResponse;
    std::string payload;
};

inline constexpr std::uint16_t ToUnderlying(MessageType type) {
    return static_cast<std::uint16_t>(type);
}

inline std::uint16_t ReadUint16BE(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8U) |
                                      static_cast<std::uint16_t>(data[1]));
}

inline std::uint32_t ReadUint32BE(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24U) |
           (static_cast<std::uint32_t>(data[1]) << 16U) |
           (static_cast<std::uint32_t>(data[2]) << 8U) |
           static_cast<std::uint32_t>(data[3]);
}

inline void WriteUint16BE(std::uint16_t value, std::uint8_t* data) {
    data[0] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    data[1] = static_cast<std::uint8_t>(value & 0xFFU);
}

inline void WriteUint32BE(std::uint32_t value, std::uint8_t* data) {
    data[0] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
    data[1] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    data[2] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    data[3] = static_cast<std::uint8_t>(value & 0xFFU);
}

inline std::array<std::uint8_t, kHeaderSize> EncodeHeader(
    MessageType type,
    std::uint32_t payload_length) {
    std::array<std::uint8_t, kHeaderSize> header{};
    WriteUint16BE(kMagic, header.data());
    header[2] = kVersion;
    WriteUint16BE(ToUnderlying(type), header.data() + 3);
    WriteUint32BE(payload_length, header.data() + 5);
    return header;
}

inline bool DecodeHeader(
    const std::uint8_t* data,
    std::size_t size,
    PacketHeader* header,
    std::string* error_text) {
    if (header == nullptr) {
        if (error_text != nullptr) {
            *error_text = "内部错误：header 输出参数为空";
        }
        return false;
    }
    if (size < kHeaderSize) {
        if (error_text != nullptr) {
            *error_text = "包头长度不足";
        }
        return false;
    }

    PacketHeader parsed;
    parsed.magic = ReadUint16BE(data);
    parsed.version = data[2];
    parsed.message_type = static_cast<MessageType>(ReadUint16BE(data + 3));
    parsed.payload_length = ReadUint32BE(data + 5);

    if (parsed.magic != kMagic) {
        if (error_text != nullptr) {
            *error_text = "魔数错误";
        }
        return false;
    }
    if (parsed.version != kVersion) {
        if (error_text != nullptr) {
            *error_text = "版本号不匹配";
        }
        return false;
    }
    if (parsed.payload_length > kMaxPayloadLength) {
        if (error_text != nullptr) {
            *error_text = "负载长度超过上限";
        }
        return false;
    }

    *header = parsed;
    return true;
}

inline std::string EncodePacket(const Packet& packet) {
    const auto payload_length = static_cast<std::uint32_t>(packet.payload.size());
    const auto header = EncodeHeader(packet.message_type, payload_length);

    std::string bytes;
    bytes.resize(kHeaderSize + packet.payload.size());
    std::memcpy(bytes.data(), header.data(), header.size());
    if (!packet.payload.empty()) {
        std::memcpy(bytes.data() + kHeaderSize, packet.payload.data(), packet.payload.size());
    }
    return bytes;
}

inline const char* MessageTypeName(MessageType type) {
    switch (type) {
    case MessageType::kPingRequest:
        return "PingRequest";
    case MessageType::kPingResponse:
        return "PingResponse";
    case MessageType::kQuitRequest:
        return "QuitRequest";
    case MessageType::kByeResponse:
        return "ByeResponse";
    case MessageType::kErrorResponse:
        return "ErrorResponse";
    case MessageType::kPredictRequest:
        return "PredictRequest";
    case MessageType::kPredictResponse:
        return "PredictResponse";
    case MessageType::kListSamplesRequest:
        return "ListSamplesRequest";
    case MessageType::kListSamplesResponse:
        return "ListSamplesResponse";
    case MessageType::kPlaySampleRequest:
        return "PlaySampleRequest";
    case MessageType::kPlaySampleResponse:
        return "PlaySampleResponse";
    case MessageType::kLoginRequest:
        return "LoginRequest";
    case MessageType::kLoginResponse:
        return "LoginResponse";
    case MessageType::kListPatientsRequest:
        return "ListPatientsRequest";
    case MessageType::kListPatientsResponse:
        return "ListPatientsResponse";
    case MessageType::kGetPatientRequest:
        return "GetPatientRequest";
    case MessageType::kGetPatientResponse:
        return "GetPatientResponse";
    case MessageType::kAddPatientRequest:
        return "AddPatientRequest";
    case MessageType::kAddPatientResponse:
        return "AddPatientResponse";
    case MessageType::kUpdatePatientRequest:
        return "UpdatePatientRequest";
    case MessageType::kUpdatePatientResponse:
        return "UpdatePatientResponse";
    case MessageType::kDeletePatientRequest:
        return "DeletePatientRequest";
    case MessageType::kDeletePatientResponse:
        return "DeletePatientResponse";
    case MessageType::kAddMonitorRecordRequest:
        return "AddMonitorRecordRequest";
    case MessageType::kAddMonitorRecordResponse:
        return "AddMonitorRecordResponse";
    case MessageType::kListMonitorRecordsRequest:
        return "ListMonitorRecordsRequest";
    case MessageType::kListMonitorRecordsResponse:
        return "ListMonitorRecordsResponse";
    case MessageType::kListAllMonitorRecordsRequest:
        return "ListAllMonitorRecordsRequest";
    case MessageType::kListAllMonitorRecordsResponse:
        return "ListAllMonitorRecordsResponse";
    case MessageType::kListAlertsRequest:
        return "ListAlertsRequest";
    case MessageType::kListAlertsResponse:
        return "ListAlertsResponse";
    case MessageType::kListAllAlertsRequest:
        return "ListAllAlertsRequest";
    case MessageType::kListAllAlertsResponse:
        return "ListAllAlertsResponse";
    case MessageType::kConfirmAlertRequest:
        return "ConfirmAlertRequest";
    case MessageType::kConfirmAlertResponse:
        return "ConfirmAlertResponse";
    case MessageType::kChangePasswordRequest:
        return "ChangePasswordRequest";
    case MessageType::kChangePasswordResponse:
        return "ChangePasswordResponse";
    default:
        return "Unknown";
    }
}

}  // namespace gp::protocol

#endif  // GP_SHARED_PROTOCOL_MESSAGE_PROTOCOL_H_
