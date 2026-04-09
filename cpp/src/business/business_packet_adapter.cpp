#include "business/business_packet_adapter.h"

#include "business/business_types.h"
#include "net/json_utils.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using gp::backend::AlertRecord;
using gp::backend::MonitorRecord;
using gp::backend::Patient;
using gp::json::ptree;

bool StartsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string Trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

gp::protocol::Packet BuildErrorPacket(const std::string& code, const std::string& message) {
    ptree root;
    root.put("code", code);
    root.put("message", message);
    return {gp::protocol::MessageType::kErrorResponse, gp::json::Serialize(root)};
}

gp::protocol::Packet BuildPacket(gp::protocol::MessageType type, const ptree& root) {
    return {type, gp::json::Serialize(root)};
}

std::vector<std::string> Split(const std::string& text, char delimiter) {
    std::vector<std::string> fields;
    std::stringstream stream(text);
    std::string field;
    while (std::getline(stream, field, delimiter)) {
        fields.push_back(field);
    }
    if (!text.empty() && text.back() == delimiter) {
        fields.emplace_back();
    }
    return fields;
}

std::string EscapeField(const std::string& text) {
    std::string sanitized = text;
    std::replace(sanitized.begin(), sanitized.end(), '|', '/');
    std::replace(sanitized.begin(), sanitized.end(), ';', ' ');
    std::replace(sanitized.begin(), sanitized.end(), '\r', ' ');
    std::replace(sanitized.begin(), sanitized.end(), '\n', ' ');
    return sanitized;
}

bool ReadRequiredString(const ptree& root, const std::string& key, std::string* value, std::string* error_text) {
    const auto field = root.get_optional<std::string>(key);
    if (!field.has_value()) {
        if (error_text != nullptr) {
            *error_text = "缺少字段: " + key;
        }
        return false;
    }
    *value = field.value();
    return true;
}

bool ReadRequiredInt(const ptree& root, const std::string& key, int* value, std::string* error_text) {
    try {
        *value = root.get<int>(key);
        return true;
    } catch (const std::exception&) {
        if (error_text != nullptr) {
            *error_text = "字段不是合法整数: " + key;
        }
        return false;
    }
}

bool ParsePatientRecord(const std::string& text, Patient* patient) {
    if (patient == nullptr) {
        return false;
    }

    const auto fields = Split(text, '|');
    if (fields.size() < 6) {
        return false;
    }

    try {
        patient->age = std::stoi(fields[3]);
    } catch (const std::exception&) {
        return false;
    }

    patient->patient_id = fields[0];
    patient->name = fields[1];
    patient->gender = fields[2];
    patient->phone = fields[4];
    patient->remark = fields[5];
    return !patient->patient_id.empty();
}

bool ParseMonitorRecord(const std::string& text, MonitorRecord* record) {
    if (record == nullptr) {
        return false;
    }

    const auto fields = Split(text, '|');
    if (fields.size() < 10) {
        return false;
    }

    record->record_id = fields[0];
    record->patient_id = fields[1];
    record->recorded_at = fields[2];
    record->pred_label = fields[3];
    record->confidence = fields[4];
    record->alert_level = fields[5];
    record->latency_ms = fields[6];
    record->source = fields[7];
    record->sample_name = fields[8];
    record->true_label = fields[9];
    return !record->record_id.empty();
}

bool ParseAlertRecord(const std::string& text, AlertRecord* alert) {
    if (alert == nullptr) {
        return false;
    }

    const auto fields = Split(text, '|');
    if (fields.size() < 9) {
        return false;
    }

    alert->alert_id = fields[0];
    alert->patient_id = fields[1];
    alert->created_at = fields[2];
    alert->alert_level = fields[3];
    alert->pred_label = fields[4];
    alert->confidence = fields[5];
    alert->source = fields[6];
    alert->sample_name = fields[7];
    alert->status = fields[8];
    if (fields.size() >= 11) {
        alert->confirmed_at = fields[9];
        alert->confirmed_by = fields[10];
    }
    return !alert->alert_id.empty();
}

ptree PatientToTree(const Patient& patient) {
    ptree root;
    root.put("patient_id", patient.patient_id);
    root.put("name", patient.name);
    root.put("gender", patient.gender);
    root.put("age", patient.age);
    root.put("phone", patient.phone);
    root.put("remark", patient.remark);
    return root;
}

ptree MonitorRecordToTree(const MonitorRecord& record) {
    ptree root;
    root.put("record_id", record.record_id);
    root.put("patient_id", record.patient_id);
    root.put("recorded_at", record.recorded_at);
    root.put("pred_label", record.pred_label);
    root.put("confidence", record.confidence);
    root.put("alert_level", record.alert_level);
    root.put("latency_ms", record.latency_ms);
    root.put("source", record.source);
    root.put("sample_name", record.sample_name);
    root.put("true_label", record.true_label);
    return root;
}

ptree AlertToTree(const AlertRecord& alert) {
    ptree root;
    root.put("alert_id", alert.alert_id);
    root.put("patient_id", alert.patient_id);
    root.put("created_at", alert.created_at);
    root.put("alert_level", alert.alert_level);
    root.put("pred_label", alert.pred_label);
    root.put("confidence", alert.confidence);
    root.put("source", alert.source);
    root.put("sample_name", alert.sample_name);
    root.put("status", alert.status == "new" ? "pending" : alert.status);
    root.put("confirmed_at", alert.confirmed_at);
    root.put("confirmed_by", alert.confirmed_by);
    return root;
}

bool ParseLoginResponse(const std::string& line, std::string* role, std::string* display_name) {
    static const std::regex kLoginRe(R"(^LOGIN_OK\s+role=(\S+)\s+display_name=(.+)$)");
    std::smatch match;
    if (!std::regex_match(line, match, kLoginRe)) {
        return false;
    }
    *role = match[1].str();
    *display_name = match[2].str();
    return true;
}

std::optional<gp::protocol::Packet> CallAndCheck(
    const std::function<std::string(const std::string&, bool*)>& line_handler,
    const std::string& line,
    const std::string& error_code,
    bool* close_conn,
    std::string* response_line) {
    *response_line = line_handler(line, close_conn);
    if (StartsWith(*response_line, "ERR ")) {
        return BuildErrorPacket(error_code, Trim(response_line->substr(4)));
    }
    return std::nullopt;
}

}  // namespace

namespace gp::backend {

gp::protocol::Packet HandleBusinessPacket(
    gp::protocol::MessageType message_type,
    const std::string& payload,
    const std::function<std::string(const std::string&, bool*)>& line_handler,
    bool* close_conn) {
    if (close_conn != nullptr) {
        *close_conn = false;
    }

    ptree payload_root;
    std::string error_text;
    if (message_type != gp::protocol::MessageType::kPingRequest &&
        message_type != gp::protocol::MessageType::kQuitRequest &&
        !gp::json::ParseObject(payload, &payload_root, &error_text)) {
        return BuildErrorPacket("invalid_json", error_text);
    }

    switch (message_type) {
    case gp::protocol::MessageType::kPingRequest: {
        const std::string response_line = line_handler("PING", close_conn);
        ptree root;
        root.put("message", response_line);
        return BuildPacket(gp::protocol::MessageType::kPingResponse, root);
    }
    case gp::protocol::MessageType::kQuitRequest: {
        const std::string response_line = line_handler("QUIT", close_conn);
        ptree root;
        root.put("message", response_line);
        return BuildPacket(gp::protocol::MessageType::kByeResponse, root);
    }
    case gp::protocol::MessageType::kLoginRequest: {
        std::string username;
        std::string password;
        if (!ReadRequiredString(payload_root, "username", &username, &error_text) ||
            !ReadRequiredString(payload_root, "password", &password, &error_text)) {
            return BuildErrorPacket("bad_request", error_text);
        }

        std::string response_line;
        const auto error_packet = CallAndCheck(line_handler, "LOGIN " + username + " " + password, "login_failed", close_conn, &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }

        std::string role;
        std::string display_name;
        if (!ParseLoginResponse(response_line, &role, &display_name)) {
            return BuildErrorPacket("invalid_response", "登录响应格式无法解析");
        }

        ptree root;
        root.put("username", username);
        root.put("role", role);
        root.put("display_name", display_name);
        return BuildPacket(gp::protocol::MessageType::kLoginResponse, root);
    }
    case gp::protocol::MessageType::kListPatientsRequest: {
        std::string response_line;
        const auto error_packet = CallAndCheck(line_handler, "LIST_PATIENTS", "list_patients_failed", close_conn, &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }
        if (!StartsWith(response_line, "PATIENTS")) {
            return BuildErrorPacket("invalid_response", "病人列表响应格式无法解析");
        }

        ptree root;
        ptree patients;
        const std::string body = Trim(response_line.substr(std::string("PATIENTS").size()));
        if (!body.empty()) {
            for (const auto& item : Split(body, ';')) {
                Patient patient;
                if (ParsePatientRecord(item, &patient)) {
                    patients.push_back(std::make_pair("", PatientToTree(patient)));
                }
            }
        }
        root.add_child("patients", patients);
        return BuildPacket(gp::protocol::MessageType::kListPatientsResponse, root);
    }
    case gp::protocol::MessageType::kGetPatientRequest: {
        std::string patient_id;
        if (!ReadRequiredString(payload_root, "patient_id", &patient_id, &error_text)) {
            return BuildErrorPacket("bad_request", error_text);
        }

        std::string response_line;
        const auto error_packet = CallAndCheck(line_handler, "GET_PATIENT " + patient_id, "get_patient_failed", close_conn, &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }
        if (!StartsWith(response_line, "PATIENT ")) {
            return BuildErrorPacket("invalid_response", "病人详情响应格式无法解析");
        }

        Patient patient;
        if (!ParsePatientRecord(response_line.substr(std::string("PATIENT ").size()), &patient)) {
            return BuildErrorPacket("invalid_response", "病人详情字段无法解析");
        }

        ptree root;
        root.add_child("patient", PatientToTree(patient));
        return BuildPacket(gp::protocol::MessageType::kGetPatientResponse, root);
    }
    case gp::protocol::MessageType::kAddPatientRequest:
    case gp::protocol::MessageType::kUpdatePatientRequest: {
        std::string patient_id;
        std::string name;
        std::string gender;
        int age = 0;
        if (!ReadRequiredString(payload_root, "patient_id", &patient_id, &error_text) ||
            !ReadRequiredString(payload_root, "name", &name, &error_text) ||
            !ReadRequiredString(payload_root, "gender", &gender, &error_text) ||
            !ReadRequiredInt(payload_root, "age", &age, &error_text)) {
            return BuildErrorPacket("bad_request", error_text);
        }
        const std::string phone = payload_root.get<std::string>("phone", "");
        const std::string remark = payload_root.get<std::string>("remark", "");

        std::ostringstream command;
        command << (message_type == gp::protocol::MessageType::kAddPatientRequest ? "ADD_PATIENT " : "UPDATE_PATIENT ")
                << EscapeField(patient_id) << '|'
                << EscapeField(name) << '|'
                << EscapeField(gender) << '|'
                << age << '|'
                << EscapeField(phone) << '|'
                << EscapeField(remark);

        std::string response_line;
        const auto error_packet = CallAndCheck(
            line_handler,
            command.str(),
            message_type == gp::protocol::MessageType::kAddPatientRequest ? "add_patient_failed" : "update_patient_failed",
            close_conn,
            &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }

        ptree root;
        root.put("message", Trim(response_line.substr(std::string("OK ").size())));
        return BuildPacket(
            message_type == gp::protocol::MessageType::kAddPatientRequest
                ? gp::protocol::MessageType::kAddPatientResponse
                : gp::protocol::MessageType::kUpdatePatientResponse,
            root);
    }
    case gp::protocol::MessageType::kDeletePatientRequest: {
        std::string patient_id;
        if (!ReadRequiredString(payload_root, "patient_id", &patient_id, &error_text)) {
            return BuildErrorPacket("bad_request", error_text);
        }

        std::string response_line;
        const auto error_packet = CallAndCheck(line_handler, "DELETE_PATIENT " + patient_id, "delete_patient_failed", close_conn, &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }

        ptree root;
        root.put("message", Trim(response_line.substr(std::string("OK ").size())));
        return BuildPacket(gp::protocol::MessageType::kDeletePatientResponse, root);
    }
    case gp::protocol::MessageType::kAddMonitorRecordRequest: {
        const std::string patient_id = payload_root.get<std::string>("patient_id", "");
        if (patient_id.empty()) {
            return BuildErrorPacket("bad_request", "缺少字段: patient_id");
        }

        std::ostringstream command;
        command << "ADD_MONITOR_RECORD "
                << EscapeField(patient_id) << '|'
                << EscapeField(payload_root.get<std::string>("pred_label", "")) << '|'
                << EscapeField(payload_root.get<std::string>("confidence", "")) << '|'
                << EscapeField(payload_root.get<std::string>("alert_level", "")) << '|'
                << EscapeField(payload_root.get<std::string>("latency_ms", "")) << '|'
                << EscapeField(payload_root.get<std::string>("source", "")) << '|'
                << EscapeField(payload_root.get<std::string>("sample_name", "")) << '|'
                << EscapeField(payload_root.get<std::string>("true_label", ""));

        std::string response_line;
        const auto error_packet = CallAndCheck(line_handler, command.str(), "add_monitor_record_failed", close_conn, &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }

        ptree root;
        root.put("message", Trim(response_line.substr(std::string("RECORD_OK ").size())));
        root.put("alert_created", response_line.find("报警") != std::string::npos);
        return BuildPacket(gp::protocol::MessageType::kAddMonitorRecordResponse, root);
    }
    case gp::protocol::MessageType::kListMonitorRecordsRequest:
    case gp::protocol::MessageType::kListAlertsRequest: {
        std::string patient_id;
        if (!ReadRequiredString(payload_root, "patient_id", &patient_id, &error_text)) {
            return BuildErrorPacket("bad_request", error_text);
        }

        const bool list_records = message_type == gp::protocol::MessageType::kListMonitorRecordsRequest;
        const std::string command = std::string(list_records ? "LIST_MONITOR_RECORDS " : "LIST_ALERTS ") + patient_id;
        std::string response_line;
        const auto error_packet = CallAndCheck(
            line_handler,
            command,
            list_records ? "list_monitor_records_failed" : "list_alerts_failed",
            close_conn,
            &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }

        const std::string prefix = list_records ? "MONITOR_RECORDS" : "ALERTS";
        if (!StartsWith(response_line, prefix)) {
            return BuildErrorPacket("invalid_response", list_records ? "监测记录响应格式无法解析" : "报警记录响应格式无法解析");
        }

        ptree root;
        ptree array_root;
        const std::string body = Trim(response_line.substr(prefix.size()));
        if (!body.empty()) {
            for (const auto& item : Split(body, ';')) {
                if (list_records) {
                    MonitorRecord record;
                    if (ParseMonitorRecord(item, &record)) {
                        array_root.push_back(std::make_pair("", MonitorRecordToTree(record)));
                    }
                } else {
                    AlertRecord alert;
                    if (ParseAlertRecord(item, &alert)) {
                        array_root.push_back(std::make_pair("", AlertToTree(alert)));
                    }
                }
            }
        }
        root.add_child(list_records ? "records" : "alerts", array_root);
        return BuildPacket(
            list_records ? gp::protocol::MessageType::kListMonitorRecordsResponse
                         : gp::protocol::MessageType::kListAlertsResponse,
            root);
    }
    case gp::protocol::MessageType::kListAllMonitorRecordsRequest:
    case gp::protocol::MessageType::kListAllAlertsRequest: {
        const bool list_records = message_type == gp::protocol::MessageType::kListAllMonitorRecordsRequest;
        const std::string command = list_records ? "LIST_ALL_MONITOR_RECORDS" : "LIST_ALL_ALERTS";
        std::string response_line;
        const auto error_packet = CallAndCheck(
            line_handler,
            command,
            list_records ? "list_all_monitor_records_failed" : "list_all_alerts_failed",
            close_conn,
            &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }

        const std::string prefix = list_records ? "ALL_MONITOR_RECORDS" : "ALL_ALERTS";
        if (!StartsWith(response_line, prefix)) {
            return BuildErrorPacket("invalid_response", list_records ? "全部监测记录响应格式无法解析" : "全部报警记录响应格式无法解析");
        }

        ptree root;
        ptree array_root;
        const std::string body = Trim(response_line.substr(prefix.size()));
        if (!body.empty()) {
            for (const auto& item : Split(body, ';')) {
                if (list_records) {
                    MonitorRecord record;
                    if (ParseMonitorRecord(item, &record)) {
                        array_root.push_back(std::make_pair("", MonitorRecordToTree(record)));
                    }
                } else {
                    AlertRecord alert;
                    if (ParseAlertRecord(item, &alert)) {
                        array_root.push_back(std::make_pair("", AlertToTree(alert)));
                    }
                }
            }
        }
        root.add_child(list_records ? "records" : "alerts", array_root);
        return BuildPacket(
            list_records ? gp::protocol::MessageType::kListAllMonitorRecordsResponse
                         : gp::protocol::MessageType::kListAllAlertsResponse,
            root);
    }
    case gp::protocol::MessageType::kConfirmAlertRequest: {
        std::string alert_id;
        std::string confirmed_by;
        if (!ReadRequiredString(payload_root, "alert_id", &alert_id, &error_text) ||
            !ReadRequiredString(payload_root, "confirmed_by", &confirmed_by, &error_text)) {
            return BuildErrorPacket("bad_request", error_text);
        }

        std::string response_line;
        const auto error_packet = CallAndCheck(
            line_handler,
            "CONFIRM_ALERT " + EscapeField(alert_id) + "|" + EscapeField(confirmed_by),
            "confirm_alert_failed",
            close_conn,
            &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }

        ptree root;
        root.put("message", Trim(response_line.substr(std::string("ALERT_OK ").size())));
        return BuildPacket(gp::protocol::MessageType::kConfirmAlertResponse, root);
    }
    case gp::protocol::MessageType::kChangePasswordRequest: {
        std::string username;
        std::string old_password;
        std::string new_password;
        if (!ReadRequiredString(payload_root, "username", &username, &error_text) ||
            !ReadRequiredString(payload_root, "old_password", &old_password, &error_text) ||
            !ReadRequiredString(payload_root, "new_password", &new_password, &error_text)) {
            return BuildErrorPacket("bad_request", error_text);
        }

        std::string response_line;
        const auto error_packet = CallAndCheck(
            line_handler,
            "CHANGE_PASSWORD " + EscapeField(username) + "|" + EscapeField(old_password) + "|" + EscapeField(new_password),
            "change_password_failed",
            close_conn,
            &response_line);
        if (error_packet.has_value()) {
            return error_packet.value();
        }

        ptree root;
        root.put("message", Trim(response_line.substr(std::string("PASSWORD_OK ").size())));
        return BuildPacket(gp::protocol::MessageType::kChangePasswordResponse, root);
    }
    default:
        return BuildErrorPacket(
            "unsupported_type",
            std::string("业务服务不支持的消息类型: ") + gp::protocol::MessageTypeName(message_type));
    }
}

}  // namespace gp::backend
