#pragma once

#include <string>

namespace gp::backend {

struct User {
    std::string username;
    std::string password;
    std::string role;
    std::string display_name;
};

struct Patient {
    std::string patient_id;
    std::string name;
    std::string gender;
    int age = 0;
    std::string phone;
    std::string remark;
};

struct MonitorRecord {
    std::string record_id;
    std::string patient_id;
    std::string recorded_at;
    std::string pred_label;
    std::string confidence;
    std::string alert_level;
    std::string latency_ms;
    std::string source;
    std::string sample_name;
    std::string true_label;
};

struct AlertRecord {
    std::string alert_id;
    std::string patient_id;
    std::string created_at;
    std::string alert_level;
    std::string pred_label;
    std::string confidence;
    std::string source;
    std::string sample_name;
    std::string status;
    std::string confirmed_at;
    std::string confirmed_by;
};

}  // namespace gp::backend
