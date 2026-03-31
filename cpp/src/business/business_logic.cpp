#include "business/business_logic.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace gp::backend {

BusinessLogic::BusinessLogic(std::shared_ptr<FileRepository> repository)
    : repository_(std::move(repository)) {
    Register("PING", [](const std::string&, bool*) {
        return std::string("PONG");
    });

    Register("QUIT", [](const std::string&, bool* close_conn) {
        if (close_conn != nullptr) {
            *close_conn = true;
        }
        return std::string("BYE");
    });

    Register("LOGIN", [this](const std::string& payload, bool*) {
        std::stringstream stream(payload);
        std::string username;
        std::string password;
        stream >> username >> password;
        if (username.empty() || password.empty()) {
            return std::string("ERR 用法: LOGIN <username> <password>");
        }

        User user;
        if (!repository_->ValidateLogin(username, password, &user)) {
            return std::string("ERR 用户名或密码错误");
        }

        std::ostringstream response;
        response << "LOGIN_OK role=" << user.role << " display_name=" << user.display_name;
        return response.str();
    });

    Register("LIST_PATIENTS", [this](const std::string&, bool*) {
        const auto patients = repository_->ListPatients();
        std::ostringstream response;
        response << "PATIENTS";
        if (!patients.empty()) {
            response << ' ';
            for (std::size_t i = 0; i < patients.size(); ++i) {
                if (i > 0) {
                    response << ';';
                }
                response << SerializePatient(patients[i]);
            }
        }
        return response.str();
    });

    Register("GET_PATIENT", [this](const std::string& payload, bool*) {
        const std::string patient_id = Trim(payload);
        if (patient_id.empty()) {
            return std::string("ERR 用法: GET_PATIENT <patient_id>");
        }

        const auto patient = repository_->GetPatientById(patient_id);
        if (!patient.has_value()) {
            return std::string("ERR 病人不存在");
        }

        return std::string("PATIENT ") + SerializePatient(patient.value());
    });

    Register("ADD_PATIENT", [this](const std::string& payload, bool*) {
        const auto fields = SplitByDelimiter(payload, '|');
        if (fields.size() < 6) {
            return std::string("ERR 用法: ADD_PATIENT id|name|gender|age|phone|remark");
        }

        Patient patient;
        patient.patient_id = Trim(fields[0]);
        patient.name = Trim(fields[1]);
        patient.gender = Trim(fields[2]);
        patient.phone = Trim(fields[4]);
        patient.remark = Trim(fields[5]);
        try {
            patient.age = std::stoi(Trim(fields[3]));
        } catch (const std::exception&) {
            return std::string("ERR 年龄必须是整数");
        }

        if (patient.patient_id.empty()) {
            return std::string("ERR 病人编号不能为空");
        }
        if (patient.name.empty()) {
            return std::string("ERR 病人姓名不能为空");
        }

        std::string err;
        if (!repository_->AddPatient(patient, &err)) {
            return std::string("ERR ") + err;
        }
        return std::string("OK 病人已添加");
    });

    Register("UPDATE_PATIENT", [this](const std::string& payload, bool*) {
        const auto fields = SplitByDelimiter(payload, '|');
        if (fields.size() < 6) {
            return std::string("ERR 用法: UPDATE_PATIENT id|name|gender|age|phone|remark");
        }

        Patient patient;
        patient.patient_id = Trim(fields[0]);
        patient.name = Trim(fields[1]);
        patient.gender = Trim(fields[2]);
        patient.phone = Trim(fields[4]);
        patient.remark = Trim(fields[5]);
        try {
            patient.age = std::stoi(Trim(fields[3]));
        } catch (const std::exception&) {
            return std::string("ERR 年龄必须是整数");
        }

        if (patient.patient_id.empty()) {
            return std::string("ERR 病人编号不能为空");
        }
        if (patient.name.empty()) {
            return std::string("ERR 病人姓名不能为空");
        }

        std::string err;
        if (!repository_->UpdatePatient(patient, &err)) {
            return std::string("ERR ") + err;
        }
        return std::string("OK 病人信息已更新");
    });

    Register("DELETE_PATIENT", [this](const std::string& payload, bool*) {
        const std::string patient_id = Trim(payload);
        if (patient_id.empty()) {
            return std::string("ERR 用法: DELETE_PATIENT <patient_id>");
        }

        std::string err;
        if (!repository_->DeletePatient(patient_id, &err)) {
            return std::string("ERR ") + err;
        }
        return std::string("OK 病人已删除");
    });

    Register("ADD_MONITOR_RECORD", [this](const std::string& payload, bool*) {
        const auto fields = SplitByDelimiter(payload, '|');
        if (fields.size() < 8) {
            return std::string(
                "ERR 用法: ADD_MONITOR_RECORD "
                "patient_id|pred_label|confidence|"
                "alert_level|latency_ms|source|"
                "sample_name|true_label");
        }

        MonitorRecord record;
        record.patient_id = Trim(FieldAt(fields, 0));
        record.pred_label = Trim(FieldAt(fields, 1));
        record.confidence = Trim(FieldAt(fields, 2));
        record.alert_level = Trim(FieldAt(fields, 3));
        record.latency_ms = Trim(FieldAt(fields, 4));
        record.source = Trim(FieldAt(fields, 5));
        record.sample_name = Trim(FieldAt(fields, 6));
        record.true_label = Trim(FieldAt(fields, 7));

        bool alert_created = false;
        std::string err;
        if (!repository_->AddMonitorRecord(record, &err, &alert_created)) {
            return std::string("ERR ") + err;
        }

        return alert_created ? std::string("RECORD_OK 监测记录已保存，并生成报警记录")
                             : std::string("RECORD_OK 监测记录已保存");
    });

    Register("LIST_MONITOR_RECORDS", [this](const std::string& payload, bool*) {
        const std::string patient_id = Trim(payload);
        if (patient_id.empty()) {
            return std::string("ERR 用法: LIST_MONITOR_RECORDS <patient_id>");
        }

        const auto records = repository_->ListMonitorRecords(patient_id);
        std::ostringstream response;
        response << "MONITOR_RECORDS";
        if (!records.empty()) {
            response << ' ';
            for (std::size_t i = 0; i < records.size(); ++i) {
                if (i > 0) {
                    response << ';';
                }
                response << SerializeMonitorRecord(records[i]);
            }
        }
        return response.str();
    });

    Register("LIST_ALL_MONITOR_RECORDS", [this](const std::string&, bool*) {
        const auto records = repository_->ListAllMonitorRecords();
        std::ostringstream response;
        response << "ALL_MONITOR_RECORDS";
        if (!records.empty()) {
            response << ' ';
            for (std::size_t i = 0; i < records.size(); ++i) {
                if (i > 0) {
                    response << ';';
                }
                response << SerializeMonitorRecord(records[i]);
            }
        }
        return response.str();
    });

    Register("LIST_ALERTS", [this](const std::string& payload, bool*) {
        const std::string patient_id = Trim(payload);
        if (patient_id.empty()) {
            return std::string("ERR 用法: LIST_ALERTS <patient_id>");
        }

        const auto alerts = repository_->ListAlerts(patient_id);
        std::ostringstream response;
        response << "ALERTS";
        if (!alerts.empty()) {
            response << ' ';
            for (std::size_t i = 0; i < alerts.size(); ++i) {
                if (i > 0) {
                    response << ';';
                }
                response << SerializeAlert(alerts[i]);
            }
        }
        return response.str();
    });

    Register("LIST_ALL_ALERTS", [this](const std::string&, bool*) {
        const auto alerts = repository_->ListAllAlerts();
        std::ostringstream response;
        response << "ALL_ALERTS";
        if (!alerts.empty()) {
            response << ' ';
            for (std::size_t i = 0; i < alerts.size(); ++i) {
                if (i > 0) {
                    response << ';';
                }
                response << SerializeAlert(alerts[i]);
            }
        }
        return response.str();
    });

    Register("CHANGE_PASSWORD", [this](const std::string& payload, bool*) {
        const auto fields = SplitByDelimiter(payload, '|');
        if (fields.size() < 3) {
            return std::string("ERR 用法: CHANGE_PASSWORD username|old_password|new_password");
        }

        const std::string username = Trim(FieldAt(fields, 0));
        const std::string old_password = Trim(FieldAt(fields, 1));
        const std::string new_password = Trim(FieldAt(fields, 2));

        if (username.empty()) {
            return std::string("ERR 用户名不能为空");
        }
        if (new_password.empty()) {
            return std::string("ERR 新密码不能为空");
        }

        std::string err;
        if (!repository_->ChangePassword(username, old_password, new_password, &err)) {
            return std::string("ERR ") + err;
        }
        return std::string("PASSWORD_OK 密码修改成功");
    });

    Register("CONFIRM_ALERT", [this](const std::string& payload, bool*) {
        const auto fields = SplitByDelimiter(payload, '|');
        if (fields.size() < 2) {
            return std::string("ERR 用法: CONFIRM_ALERT alert_id|confirmed_by");
        }

        const std::string alert_id = Trim(FieldAt(fields, 0));
        const std::string confirmed_by = Trim(FieldAt(fields, 1));
        if (alert_id.empty()) {
            return std::string("ERR 报警编号不能为空");
        }

        std::string err;
        if (!repository_->ConfirmAlert(alert_id, confirmed_by, &err)) {
            return std::string("ERR ") + err;
        }
        return std::string("ALERT_OK 报警已确认");
    });
}

std::string BusinessLogic::HandleLine(const std::string& line, bool* close_conn) const {
    if (close_conn != nullptr) {
        *close_conn = false;
    }

    std::string trimmed = line;
    while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n')) {
        trimmed.pop_back();
    }
    if (trimmed.empty()) {
        return "ERR 空命令";
    }

    std::string cmd;
    std::string payload;
    const auto pos = trimmed.find(' ');
    if (pos == std::string::npos) {
        cmd = ToUpper(trimmed);
    } else {
        cmd = ToUpper(trimmed.substr(0, pos));
        payload = trimmed.substr(pos + 1);
    }

    const auto it = handlers_.find(cmd);
    if (it == handlers_.end()) {
        return "ERR 未知命令，支持: PING | LOGIN | "
               "LIST_PATIENTS | GET_PATIENT | ADD_PATIENT | "
               "UPDATE_PATIENT | DELETE_PATIENT | "
               "ADD_MONITOR_RECORD | LIST_MONITOR_RECORDS | "
               "LIST_ALL_MONITOR_RECORDS | LIST_ALERTS | "
               "LIST_ALL_ALERTS | CONFIRM_ALERT | "
               "CHANGE_PASSWORD | QUIT";
    }
    return it->second(payload, close_conn);
}

std::string BusinessLogic::Trim(const std::string& text) {
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

std::string BusinessLogic::ToUpper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return text;
}

std::vector<std::string> BusinessLogic::SplitByDelimiter(const std::string& text, char delimiter) {
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

std::string BusinessLogic::FieldAt(const std::vector<std::string>& fields, std::size_t index) {
    return index < fields.size() ? fields[index] : std::string();
}

std::string BusinessLogic::SerializePatient(const Patient& patient) {
    std::ostringstream stream;
    stream << patient.patient_id << '|'
           << patient.name << '|'
           << patient.gender << '|'
           << patient.age << '|'
           << patient.phone << '|'
           << patient.remark;
    return stream.str();
}

std::string BusinessLogic::SerializeMonitorRecord(const MonitorRecord& record) {
    std::ostringstream stream;
    stream << record.record_id << '|'
           << record.patient_id << '|'
           << record.recorded_at << '|'
           << record.pred_label << '|'
           << record.confidence << '|'
           << record.alert_level << '|'
           << record.latency_ms << '|'
           << record.source << '|'
           << record.sample_name << '|'
           << record.true_label;
    return stream.str();
}

std::string BusinessLogic::SerializeAlert(const AlertRecord& alert) {
    std::ostringstream stream;
    stream << alert.alert_id << '|'
           << alert.patient_id << '|'
           << alert.created_at << '|'
           << alert.alert_level << '|'
           << alert.pred_label << '|'
           << alert.confidence << '|'
           << alert.source << '|'
           << alert.sample_name << '|'
           << alert.status << '|'
           << alert.confirmed_at << '|'
           << alert.confirmed_by;
    return stream.str();
}

void BusinessLogic::Register(const std::string& cmd, CommandHandler handler) {
    handlers_[cmd] = std::move(handler);
}

}  // namespace gp::backend

