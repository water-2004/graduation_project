#include "business/file_repository.h"
#include "business/sha256.h"

#include "sqlite3.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace gp::backend {

namespace {

struct StatementGuard {
    explicit StatementGuard(sqlite3_stmt* stmt) : stmt(stmt) {}
    ~StatementGuard() {
        if (stmt != nullptr) {
            sqlite3_finalize(stmt);
        }
    }
    sqlite3_stmt* stmt = nullptr;
};

std::runtime_error BuildSqliteError(sqlite3* db, const std::string& prefix) {
    const char* error_text = db != nullptr ? sqlite3_errmsg(db) : "unknown";
    return std::runtime_error(prefix + ": " + error_text);
}

void BindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    if (sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        throw std::runtime_error("SQLite 绑定字符串失败");
    }
}

std::string ColumnText(sqlite3_stmt* stmt, int index) {
    const unsigned char* value = sqlite3_column_text(stmt, index);
    return value == nullptr ? std::string() : reinterpret_cast<const char*>(value);
}

}  // namespace

FileRepository::FileRepository(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir)),
      db_file_(data_dir_ / "business_service.db"),
      users_file_(data_dir_ / "users.tsv"),
      patients_file_(data_dir_ / "patients.tsv"),
      monitor_records_file_(data_dir_ / "monitor_records.tsv"),
      alerts_file_(data_dir_ / "alerts.tsv") {}

FileRepository::~FileRepository() {
    std::scoped_lock lock(mutex_);
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void FileRepository::Initialize() {
    std::scoped_lock lock(mutex_);
    std::error_code ec;
    std::filesystem::create_directories(data_dir_, ec);
    if (ec) {
        throw std::runtime_error("创建业务数据目录失败: " + ec.message());
    }

    OpenDatabase();
    CreateSchema();
    ImportLegacyTsvIfNeeded();
    SeedDefaultUserIfNeeded();
}

bool FileRepository::ValidateLogin(
    const std::string& username,
    const std::string& password,
    User* out_user) const {
    std::scoped_lock lock(mutex_);

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "SELECT username, password, role, display_name "
        "FROM users WHERE username = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "查询用户失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, username);

    const int rc = sqlite3_step(stmt.stmt);
    if (rc == SQLITE_DONE) {
        return false;
    }
    if (rc != SQLITE_ROW) {
        throw BuildSqliteError(db_, "执行用户查询失败");
    }

    const std::string stored_password = ColumnText(stmt.stmt, 1);
    const std::string input_hash = Sha256Hex(password);
    bool matched = false;

    if (IsSha256Hex(stored_password)) {
        matched = (input_hash == stored_password);
    } else {
        matched = (password == stored_password);
        if (matched) {
            // Auto-upgrade: replace plaintext password with hash.
            sqlite3_stmt* upgrade_raw = nullptr;
            const char* upgrade_sql = "UPDATE users SET password = ? WHERE username = ?;";
            if (sqlite3_prepare_v2(db_, upgrade_sql, -1, &upgrade_raw, nullptr) == SQLITE_OK) {
                StatementGuard upgrade_stmt(upgrade_raw);
                BindText(upgrade_stmt.stmt, 1, input_hash);
                BindText(upgrade_stmt.stmt, 2, username);
                sqlite3_step(upgrade_stmt.stmt);
            }
        }
    }

    if (!matched) {
        return false;
    }

    if (out_user != nullptr) {
        out_user->username = ColumnText(stmt.stmt, 0);
        out_user->password = stored_password;
        out_user->role = ColumnText(stmt.stmt, 2);
        out_user->display_name = ColumnText(stmt.stmt, 3);
    }
    return true;
}

std::vector<Patient> FileRepository::ListPatients() const {
    std::scoped_lock lock(mutex_);
    std::vector<Patient> patients;

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "SELECT patient_id, name, gender, age, phone, remark "
        "FROM patients ORDER BY patient_id ASC;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "查询病人列表失败");
    }
    StatementGuard stmt(raw_stmt);

    while (true) {
        const int rc = sqlite3_step(stmt.stmt);
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            throw BuildSqliteError(db_, "读取病人列表失败");
        }

        Patient patient;
        patient.patient_id = ColumnText(stmt.stmt, 0);
        patient.name = ColumnText(stmt.stmt, 1);
        patient.gender = ColumnText(stmt.stmt, 2);
        patient.age = sqlite3_column_int(stmt.stmt, 3);
        patient.phone = ColumnText(stmt.stmt, 4);
        patient.remark = ColumnText(stmt.stmt, 5);
        patients.push_back(std::move(patient));
    }
    return patients;
}

std::optional<Patient> FileRepository::GetPatientById(const std::string& patient_id) const {
    std::scoped_lock lock(mutex_);

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "SELECT patient_id, name, gender, age, phone, remark "
        "FROM patients WHERE patient_id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "查询病人详情失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, patient_id);

    const int rc = sqlite3_step(stmt.stmt);
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        throw BuildSqliteError(db_, "读取病人详情失败");
    }

    Patient patient;
    patient.patient_id = ColumnText(stmt.stmt, 0);
    patient.name = ColumnText(stmt.stmt, 1);
    patient.gender = ColumnText(stmt.stmt, 2);
    patient.age = sqlite3_column_int(stmt.stmt, 3);
    patient.phone = ColumnText(stmt.stmt, 4);
    patient.remark = ColumnText(stmt.stmt, 5);
    return patient;
}

bool FileRepository::AddPatient(const Patient& patient, std::string* err) {
    std::scoped_lock lock(mutex_);
    if (patient.patient_id.empty()) {
        if (err != nullptr) {
            *err = "病人编号不能为空";
        }
        return false;
    }

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "INSERT INTO patients(patient_id, name, gender, age, phone, remark) "
        "VALUES(?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "准备新增病人语句失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, patient.patient_id);
    BindText(stmt.stmt, 2, patient.name);
    BindText(stmt.stmt, 3, patient.gender);
    if (sqlite3_bind_int(stmt.stmt, 4, patient.age) != SQLITE_OK) {
        throw BuildSqliteError(db_, "绑定病人年龄失败");
    }
    BindText(stmt.stmt, 5, patient.phone);
    BindText(stmt.stmt, 6, patient.remark);

    const int rc = sqlite3_step(stmt.stmt);
    if (rc == SQLITE_DONE) {
        return true;
    }
    if (err != nullptr) {
        *err = rc == SQLITE_CONSTRAINT ? "病人编号已存在" : sqlite3_errmsg(db_);
    }
    return false;
}

bool FileRepository::UpdatePatient(const Patient& patient, std::string* err) {
    std::scoped_lock lock(mutex_);
    if (patient.patient_id.empty()) {
        if (err != nullptr) {
            *err = "病人编号不能为空";
        }
        return false;
    }
    if (!PatientExists(patient.patient_id)) {
        if (err != nullptr) {
            *err = "病人不存在";
        }
        return false;
    }

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "UPDATE patients "
        "SET name = ?, gender = ?, age = ?, phone = ?, remark = ? "
        "WHERE patient_id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "准备更新病人语句失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, patient.name);
    BindText(stmt.stmt, 2, patient.gender);
    if (sqlite3_bind_int(stmt.stmt, 3, patient.age) != SQLITE_OK) {
        throw BuildSqliteError(db_, "绑定病人年龄失败");
    }
    BindText(stmt.stmt, 4, patient.phone);
    BindText(stmt.stmt, 5, patient.remark);
    BindText(stmt.stmt, 6, patient.patient_id);

    const int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        if (err != nullptr) {
            *err = sqlite3_errmsg(db_);
        }
        return false;
    }
    return true;
}

bool FileRepository::DeletePatient(const std::string& patient_id, std::string* err) {
    std::scoped_lock lock(mutex_);
    if (!PatientExists(patient_id)) {
        if (err != nullptr) {
            *err = "病人不存在";
        }
        return false;
    }

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql = "DELETE FROM patients WHERE patient_id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "准备删除病人语句失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, patient_id);

    const int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_DONE) {
        if (err != nullptr) {
            *err = sqlite3_errmsg(db_);
        }
        return false;
    }
    return true;
}

bool FileRepository::AddMonitorRecord(
    const MonitorRecord& record,
    std::string* err,
    bool* alert_created) {
    std::scoped_lock lock(mutex_);
    if (alert_created != nullptr) {
        *alert_created = false;
    }

    if (record.patient_id.empty()) {
        if (err != nullptr) {
            *err = "病人编号不能为空";
        }
        return false;
    }
    if (!PatientExists(record.patient_id)) {
        if (err != nullptr) {
            *err = "病人不存在，无法保存监测记录";
        }
        return false;
    }

    MonitorRecord stored_record = record;
    if (stored_record.pred_label.empty()) {
        if (err != nullptr) {
            *err = "预测类别不能为空";
        }
        return false;
    }
    if (stored_record.recorded_at.empty()) {
        stored_record.recorded_at = CurrentDateTimeText();
    }
    if (stored_record.record_id.empty()) {
        stored_record.record_id = BuildRecordId("MR", CountRows("monitor_records") + 1);
    }
    if (stored_record.source.empty()) {
        stored_record.source = "unknown";
    }

    try {
        Execute("BEGIN IMMEDIATE TRANSACTION;");

        sqlite3_stmt* raw_stmt = nullptr;
        const char* record_sql =
            "INSERT INTO monitor_records("
            "record_id, patient_id, recorded_at, pred_label, confidence, "
            "alert_level, latency_ms, source, sample_name, true_label) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db_, record_sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
            throw BuildSqliteError(db_, "准备写入监测记录语句失败");
        }
        StatementGuard stmt(raw_stmt);
        BindText(stmt.stmt, 1, stored_record.record_id);
        BindText(stmt.stmt, 2, stored_record.patient_id);
        BindText(stmt.stmt, 3, stored_record.recorded_at);
        BindText(stmt.stmt, 4, stored_record.pred_label);
        BindText(stmt.stmt, 5, stored_record.confidence);
        BindText(stmt.stmt, 6, stored_record.alert_level);
        BindText(stmt.stmt, 7, stored_record.latency_ms);
        BindText(stmt.stmt, 8, stored_record.source);
        BindText(stmt.stmt, 9, stored_record.sample_name);
        BindText(stmt.stmt, 10, stored_record.true_label);
        if (sqlite3_step(stmt.stmt) != SQLITE_DONE) {
            throw BuildSqliteError(db_, "写入监测记录失败");
        }

        if (NeedsAlert(stored_record.alert_level)) {
            AlertRecord alert;
            alert.alert_id = BuildRecordId("AL", CountRows("alerts") + 1);
            alert.patient_id = stored_record.patient_id;
            alert.created_at = stored_record.recorded_at;
            alert.alert_level = stored_record.alert_level;
            alert.pred_label = stored_record.pred_label;
            alert.confidence = stored_record.confidence;
            alert.source = stored_record.source;
            alert.sample_name = stored_record.sample_name;
            alert.status = "new";

            sqlite3_stmt* alert_raw_stmt = nullptr;
            const char* alert_sql =
                "INSERT INTO alerts("
                "alert_id, patient_id, created_at, alert_level, pred_label, confidence, "
                "source, sample_name, status, confirmed_at, confirmed_by) "
                "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
            if (sqlite3_prepare_v2(db_, alert_sql, -1, &alert_raw_stmt, nullptr) != SQLITE_OK) {
                throw BuildSqliteError(db_, "准备写入报警记录语句失败");
            }
            StatementGuard alert_stmt(alert_raw_stmt);
            BindText(alert_stmt.stmt, 1, alert.alert_id);
            BindText(alert_stmt.stmt, 2, alert.patient_id);
            BindText(alert_stmt.stmt, 3, alert.created_at);
            BindText(alert_stmt.stmt, 4, alert.alert_level);
            BindText(alert_stmt.stmt, 5, alert.pred_label);
            BindText(alert_stmt.stmt, 6, alert.confidence);
            BindText(alert_stmt.stmt, 7, alert.source);
            BindText(alert_stmt.stmt, 8, alert.sample_name);
            BindText(alert_stmt.stmt, 9, alert.status);
            BindText(alert_stmt.stmt, 10, alert.confirmed_at);
            BindText(alert_stmt.stmt, 11, alert.confirmed_by);
            if (sqlite3_step(alert_stmt.stmt) != SQLITE_DONE) {
                throw BuildSqliteError(db_, "写入报警记录失败");
            }
            if (alert_created != nullptr) {
                *alert_created = true;
            }
        }

        Execute("COMMIT;");
        return true;
    } catch (const std::exception& e) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        if (err != nullptr) {
            *err = e.what();
        }
        return false;
    }
}

std::vector<MonitorRecord> FileRepository::ListMonitorRecords(const std::string& patient_id) const {
    std::scoped_lock lock(mutex_);
    std::vector<MonitorRecord> records;

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "SELECT record_id, patient_id, recorded_at, pred_label, confidence, "
        "alert_level, latency_ms, source, sample_name, true_label "
        "FROM monitor_records WHERE patient_id = ? "
        "ORDER BY recorded_at DESC, record_id DESC;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "查询监测记录失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, patient_id);

    while (true) {
        const int rc = sqlite3_step(stmt.stmt);
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            throw BuildSqliteError(db_, "读取监测记录失败");
        }

        MonitorRecord record;
        record.record_id = ColumnText(stmt.stmt, 0);
        record.patient_id = ColumnText(stmt.stmt, 1);
        record.recorded_at = ColumnText(stmt.stmt, 2);
        record.pred_label = ColumnText(stmt.stmt, 3);
        record.confidence = ColumnText(stmt.stmt, 4);
        record.alert_level = ColumnText(stmt.stmt, 5);
        record.latency_ms = ColumnText(stmt.stmt, 6);
        record.source = ColumnText(stmt.stmt, 7);
        record.sample_name = ColumnText(stmt.stmt, 8);
        record.true_label = ColumnText(stmt.stmt, 9);
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<MonitorRecord> FileRepository::ListAllMonitorRecords() const {
    std::scoped_lock lock(mutex_);
    std::vector<MonitorRecord> records;

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "SELECT record_id, patient_id, recorded_at, pred_label, confidence, "
        "alert_level, latency_ms, source, sample_name, true_label "
        "FROM monitor_records ORDER BY recorded_at DESC, record_id DESC;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "查询全量监测记录失败");
    }
    StatementGuard stmt(raw_stmt);

    while (true) {
        const int rc = sqlite3_step(stmt.stmt);
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            throw BuildSqliteError(db_, "读取全量监测记录失败");
        }

        MonitorRecord record;
        record.record_id = ColumnText(stmt.stmt, 0);
        record.patient_id = ColumnText(stmt.stmt, 1);
        record.recorded_at = ColumnText(stmt.stmt, 2);
        record.pred_label = ColumnText(stmt.stmt, 3);
        record.confidence = ColumnText(stmt.stmt, 4);
        record.alert_level = ColumnText(stmt.stmt, 5);
        record.latency_ms = ColumnText(stmt.stmt, 6);
        record.source = ColumnText(stmt.stmt, 7);
        record.sample_name = ColumnText(stmt.stmt, 8);
        record.true_label = ColumnText(stmt.stmt, 9);
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<AlertRecord> FileRepository::ListAlerts(const std::string& patient_id) const {
    std::scoped_lock lock(mutex_);
    std::vector<AlertRecord> alerts;

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "SELECT alert_id, patient_id, created_at, alert_level, pred_label, confidence, "
        "source, sample_name, status, confirmed_at, confirmed_by "
        "FROM alerts WHERE patient_id = ? "
        "ORDER BY created_at DESC, alert_id DESC;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "查询报警记录失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, patient_id);

    while (true) {
        const int rc = sqlite3_step(stmt.stmt);
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            throw BuildSqliteError(db_, "读取报警记录失败");
        }

        AlertRecord alert;
        alert.alert_id = ColumnText(stmt.stmt, 0);
        alert.patient_id = ColumnText(stmt.stmt, 1);
        alert.created_at = ColumnText(stmt.stmt, 2);
        alert.alert_level = ColumnText(stmt.stmt, 3);
        alert.pred_label = ColumnText(stmt.stmt, 4);
        alert.confidence = ColumnText(stmt.stmt, 5);
        alert.source = ColumnText(stmt.stmt, 6);
        alert.sample_name = ColumnText(stmt.stmt, 7);
        alert.status = ColumnText(stmt.stmt, 8);
        alert.confirmed_at = ColumnText(stmt.stmt, 9);
        alert.confirmed_by = ColumnText(stmt.stmt, 10);
        alerts.push_back(std::move(alert));
    }
    return alerts;
}

std::vector<AlertRecord> FileRepository::ListAllAlerts() const {
    std::scoped_lock lock(mutex_);
    std::vector<AlertRecord> alerts;

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "SELECT alert_id, patient_id, created_at, alert_level, pred_label, confidence, "
        "source, sample_name, status, confirmed_at, confirmed_by "
        "FROM alerts ORDER BY created_at DESC, alert_id DESC;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "查询全量报警记录失败");
    }
    StatementGuard stmt(raw_stmt);

    while (true) {
        const int rc = sqlite3_step(stmt.stmt);
        if (rc == SQLITE_DONE) {
            break;
        }
        if (rc != SQLITE_ROW) {
            throw BuildSqliteError(db_, "读取全量报警记录失败");
        }

        AlertRecord alert;
        alert.alert_id = ColumnText(stmt.stmt, 0);
        alert.patient_id = ColumnText(stmt.stmt, 1);
        alert.created_at = ColumnText(stmt.stmt, 2);
        alert.alert_level = ColumnText(stmt.stmt, 3);
        alert.pred_label = ColumnText(stmt.stmt, 4);
        alert.confidence = ColumnText(stmt.stmt, 5);
        alert.source = ColumnText(stmt.stmt, 6);
        alert.sample_name = ColumnText(stmt.stmt, 7);
        alert.status = ColumnText(stmt.stmt, 8);
        alert.confirmed_at = ColumnText(stmt.stmt, 9);
        alert.confirmed_by = ColumnText(stmt.stmt, 10);
        alerts.push_back(std::move(alert));
    }
    return alerts;
}

bool FileRepository::ConfirmAlert(
    const std::string& alert_id,
    const std::string& confirmed_by,
    std::string* err) {
    std::scoped_lock lock(mutex_);

    sqlite3_stmt* query_raw_stmt = nullptr;
    const char* query_sql = "SELECT status FROM alerts WHERE alert_id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, query_sql, -1, &query_raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "准备报警查询语句失败");
    }
    StatementGuard query_stmt(query_raw_stmt);
    BindText(query_stmt.stmt, 1, alert_id);

    const int query_rc = sqlite3_step(query_stmt.stmt);
    if (query_rc == SQLITE_DONE) {
        if (err != nullptr) {
            *err = "报警记录不存在";
        }
        return false;
    }
    if (query_rc != SQLITE_ROW) {
        if (err != nullptr) {
            *err = sqlite3_errmsg(db_);
        }
        return false;
    }
    if (ColumnText(query_stmt.stmt, 0) == "confirmed") {
        if (err != nullptr) {
            *err = "该报警已确认，无需重复操作";
        }
        return false;
    }

    sqlite3_stmt* update_raw_stmt = nullptr;
    const char* update_sql =
        "UPDATE alerts SET status = 'confirmed', confirmed_at = ?, confirmed_by = ? "
        "WHERE alert_id = ?;";
    if (sqlite3_prepare_v2(db_, update_sql, -1, &update_raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "准备报警确认语句失败");
    }
    StatementGuard update_stmt(update_raw_stmt);
    BindText(update_stmt.stmt, 1, CurrentDateTimeText());
    BindText(update_stmt.stmt, 2, confirmed_by.empty() ? std::string("unknown") : confirmed_by);
    BindText(update_stmt.stmt, 3, alert_id);

    const int update_rc = sqlite3_step(update_stmt.stmt);
    if (update_rc != SQLITE_DONE) {
        if (err != nullptr) {
            *err = sqlite3_errmsg(db_);
        }
        return false;
    }
    return true;
}

bool FileRepository::ChangePassword(
    const std::string& username,
    const std::string& old_password,
    const std::string& new_password,
    std::string* err) {
    if (username.empty()) {
        if (err != nullptr) { *err = "用户名不能为空"; }
        return false;
    }
    if (new_password.empty()) {
        if (err != nullptr) { *err = "新密码不能为空"; }
        return false;
    }

    // Verify old password first (without lock, ValidateLogin takes lock itself).
    // We need to do this manually since ValidateLogin auto-upgrades.
    std::scoped_lock lock(mutex_);

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql = "SELECT password FROM users WHERE username = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "查询用户失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, username);

    const int rc = sqlite3_step(stmt.stmt);
    if (rc == SQLITE_DONE) {
        if (err != nullptr) { *err = "用户不存在"; }
        return false;
    }
    if (rc != SQLITE_ROW) {
        throw BuildSqliteError(db_, "执行用户查询失败");
    }

    const std::string stored = ColumnText(stmt.stmt, 0);
    const std::string old_hash = Sha256Hex(old_password);
    bool old_ok = false;
    if (IsSha256Hex(stored)) {
        old_ok = (old_hash == stored);
    } else {
        old_ok = (old_password == stored);
    }

    if (!old_ok) {
        if (err != nullptr) { *err = "原密码错误"; }
        return false;
    }

    sqlite3_stmt* update_raw = nullptr;
    const char* update_sql = "UPDATE users SET password = ? WHERE username = ?;";
    if (sqlite3_prepare_v2(db_, update_sql, -1, &update_raw, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "准备修改密码语句失败");
    }
    StatementGuard update_stmt(update_raw);
    BindText(update_stmt.stmt, 1, Sha256Hex(new_password));
    BindText(update_stmt.stmt, 2, username);

    if (sqlite3_step(update_stmt.stmt) != SQLITE_DONE) {
        if (err != nullptr) { *err = sqlite3_errmsg(db_); }
        return false;
    }
    return true;
}

void FileRepository::OpenDatabase() {
    if (db_ != nullptr) {
        return;
    }

    const int rc = sqlite3_open_v2(
        db_file_.string().c_str(),
        &db_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        throw BuildSqliteError(db_, "打开 SQLite 数据库失败");
    }
    sqlite3_busy_timeout(db_, 3000);
    Execute("PRAGMA foreign_keys = ON;");
}

void FileRepository::CreateSchema() {
    Execute(
        "CREATE TABLE IF NOT EXISTS users("
        "username TEXT PRIMARY KEY,"
        "password TEXT NOT NULL,"
        "role TEXT NOT NULL,"
        "display_name TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS patients("
        "patient_id TEXT PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "gender TEXT NOT NULL,"
        "age INTEGER NOT NULL,"
        "phone TEXT,"
        "remark TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS monitor_records("
        "record_id TEXT PRIMARY KEY,"
        "patient_id TEXT NOT NULL,"
        "recorded_at TEXT NOT NULL,"
        "pred_label TEXT NOT NULL,"
        "confidence TEXT,"
        "alert_level TEXT,"
        "latency_ms TEXT,"
        "source TEXT,"
        "sample_name TEXT,"
        "true_label TEXT,"
        "FOREIGN KEY(patient_id) REFERENCES patients(patient_id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS alerts("
        "alert_id TEXT PRIMARY KEY,"
        "patient_id TEXT NOT NULL,"
        "created_at TEXT NOT NULL,"
        "alert_level TEXT NOT NULL,"
        "pred_label TEXT NOT NULL,"
        "confidence TEXT,"
        "source TEXT,"
        "sample_name TEXT,"
        "status TEXT NOT NULL,"
        "confirmed_at TEXT,"
        "confirmed_by TEXT,"
        "FOREIGN KEY(patient_id) REFERENCES patients(patient_id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_monitor_records_patient_id ON monitor_records(patient_id);"
        "CREATE INDEX IF NOT EXISTS idx_alerts_patient_id ON alerts(patient_id);"
        "CREATE INDEX IF NOT EXISTS idx_alerts_status ON alerts(status);");
}

void FileRepository::SeedDefaultUserIfNeeded() {
    if (!TableIsEmpty("users")) {
        return;
    }

    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql =
        "INSERT INTO users(username, password, role, display_name) VALUES(?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "准备初始化管理员账号语句失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, "admin");
    BindText(stmt.stmt, 2, Sha256Hex("123456"));
    BindText(stmt.stmt, 3, "admin");
    BindText(stmt.stmt, 4, "系统管理员");
    if (sqlite3_step(stmt.stmt) != SQLITE_DONE) {
        throw BuildSqliteError(db_, "初始化管理员账号失败");
    }
}

void FileRepository::ImportLegacyTsvIfNeeded() {
    auto import_users = [&]() {
        if (!TableIsEmpty("users") || !std::filesystem::exists(users_file_)) {
            return;
        }
        std::ifstream input(users_file_);
        std::string line;
        Execute("BEGIN IMMEDIATE TRANSACTION;");
        try {
            while (std::getline(input, line)) {
                if (line.empty()) {
                    continue;
                }
                const auto fields = SplitTsvLine(line);
                if (fields.size() < 4) {
                    continue;
                }

                sqlite3_stmt* raw_stmt = nullptr;
                const char* sql =
                    "INSERT INTO users(username, password, role, display_name) VALUES(?, ?, ?, ?);";
                if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
                    throw BuildSqliteError(db_, "导入用户数据失败");
                }
                StatementGuard stmt(raw_stmt);
                BindText(stmt.stmt, 1, fields[0]);
                BindText(stmt.stmt, 2, IsSha256Hex(fields[1]) ? fields[1] : Sha256Hex(fields[1]));
                BindText(stmt.stmt, 3, fields[2]);
                BindText(stmt.stmt, 4, fields[3]);
                if (sqlite3_step(stmt.stmt) != SQLITE_DONE) {
                    throw BuildSqliteError(db_, "写入用户数据失败");
                }
            }
            Execute("COMMIT;");
        } catch (...) {
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    };

    auto import_patients = [&]() {
        if (!TableIsEmpty("patients") || !std::filesystem::exists(patients_file_)) {
            return;
        }
        std::ifstream input(patients_file_);
        std::string line;
        Execute("BEGIN IMMEDIATE TRANSACTION;");
        try {
            while (std::getline(input, line)) {
                if (line.empty()) {
                    continue;
                }
                const auto fields = SplitTsvLine(line);
                if (fields.size() < 6) {
                    continue;
                }

                sqlite3_stmt* raw_stmt = nullptr;
                const char* sql =
                    "INSERT INTO patients(patient_id, name, gender, age, phone, remark) VALUES(?, ?, ?, ?, ?, ?);";
                if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
                    throw BuildSqliteError(db_, "导入病人数据失败");
                }
                StatementGuard stmt(raw_stmt);
                BindText(stmt.stmt, 1, fields[0]);
                BindText(stmt.stmt, 2, fields[1]);
                BindText(stmt.stmt, 3, fields[2]);
                if (sqlite3_bind_int(stmt.stmt, 4, std::stoi(fields[3])) != SQLITE_OK) {
                    throw BuildSqliteError(db_, "导入病人年龄失败");
                }
                BindText(stmt.stmt, 5, fields[4]);
                BindText(stmt.stmt, 6, fields[5]);
                if (sqlite3_step(stmt.stmt) != SQLITE_DONE) {
                    throw BuildSqliteError(db_, "写入病人数据失败");
                }
            }
            Execute("COMMIT;");
        } catch (...) {
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    };

    auto import_monitor_records = [&]() {
        if (!TableIsEmpty("monitor_records") || !std::filesystem::exists(monitor_records_file_)) {
            return;
        }
        std::ifstream input(monitor_records_file_);
        std::string line;
        Execute("BEGIN IMMEDIATE TRANSACTION;");
        try {
            while (std::getline(input, line)) {
                if (line.empty()) {
                    continue;
                }
                const auto fields = SplitTsvLine(line);
                if (fields.size() < 10) {
                    continue;
                }

                sqlite3_stmt* raw_stmt = nullptr;
                const char* sql =
                    "INSERT INTO monitor_records(record_id, patient_id, recorded_at, pred_label, confidence, "
                    "alert_level, latency_ms, source, sample_name, true_label) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
                if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
                    throw BuildSqliteError(db_, "导入监测记录失败");
                }
                StatementGuard stmt(raw_stmt);
                for (int i = 0; i < 10; ++i) {
                    BindText(stmt.stmt, i + 1, fields[static_cast<std::size_t>(i)]);
                }
                if (sqlite3_step(stmt.stmt) != SQLITE_DONE) {
                    throw BuildSqliteError(db_, "写入监测记录失败");
                }
            }
            Execute("COMMIT;");
        } catch (...) {
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    };

    auto import_alerts = [&]() {
        if (!TableIsEmpty("alerts") || !std::filesystem::exists(alerts_file_)) {
            return;
        }
        std::ifstream input(alerts_file_);
        std::string line;
        Execute("BEGIN IMMEDIATE TRANSACTION;");
        try {
            while (std::getline(input, line)) {
                if (line.empty()) {
                    continue;
                }
                auto fields = SplitTsvLine(line);
                if (fields.size() < 9) {
                    continue;
                }
                while (fields.size() < 11) {
                    fields.push_back(std::string());
                }

                sqlite3_stmt* raw_stmt = nullptr;
                const char* sql =
                    "INSERT INTO alerts(alert_id, patient_id, created_at, alert_level, pred_label, confidence, "
                    "source, sample_name, status, confirmed_at, confirmed_by) "
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
                if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
                    throw BuildSqliteError(db_, "导入报警记录失败");
                }
                StatementGuard stmt(raw_stmt);
                for (int i = 0; i < 11; ++i) {
                    BindText(stmt.stmt, i + 1, fields[static_cast<std::size_t>(i)]);
                }
                if (sqlite3_step(stmt.stmt) != SQLITE_DONE) {
                    throw BuildSqliteError(db_, "写入报警记录失败");
                }
            }
            Execute("COMMIT;");
        } catch (...) {
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    };

    import_users();
    import_patients();
    import_monitor_records();
    import_alerts();
}

bool FileRepository::TableIsEmpty(const char* table_name) const {
    return CountRows(table_name) == 0;
}

std::size_t FileRepository::CountRows(const char* table_name) const {
    std::string sql = std::string("SELECT COUNT(*) FROM ") + table_name + ';';
    sqlite3_stmt* raw_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, std::string("统计表数据失败: ") + table_name);
    }
    StatementGuard stmt(raw_stmt);

    const int rc = sqlite3_step(stmt.stmt);
    if (rc != SQLITE_ROW) {
        throw BuildSqliteError(db_, std::string("读取表统计结果失败: ") + table_name);
    }
    return static_cast<std::size_t>(sqlite3_column_int64(stmt.stmt, 0));
}

bool FileRepository::PatientExists(const std::string& patient_id) const {
    sqlite3_stmt* raw_stmt = nullptr;
    const char* sql = "SELECT 1 FROM patients WHERE patient_id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw BuildSqliteError(db_, "准备病人存在性查询失败");
    }
    StatementGuard stmt(raw_stmt);
    BindText(stmt.stmt, 1, patient_id);
    const int rc = sqlite3_step(stmt.stmt);
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    throw BuildSqliteError(db_, "执行病人存在性查询失败");
}

void FileRepository::Execute(const char* sql) const {
    char* error_message = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &error_message);
    if (rc != SQLITE_OK) {
        std::string text = error_message != nullptr ? error_message : "unknown";
        sqlite3_free(error_message);
        throw std::runtime_error("执行 SQLite 语句失败: " + text);
    }
}

std::vector<std::string> FileRepository::SplitTsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, '\t')) {
        fields.push_back(field);
    }
    if (!line.empty() && line.back() == '\t') {
        fields.emplace_back();
    }
    return fields;
}

std::string FileRepository::SanitizeField(const std::string& value) {
    std::string result = value;
    std::replace(result.begin(), result.end(), '\t', ' ');
    std::replace(result.begin(), result.end(), '\n', ' ');
    std::replace(result.begin(), result.end(), '\r', ' ');
    return result;
}

std::string FileRepository::CurrentDateTimeText() {
    const std::time_t now = std::time(nullptr);
    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &now);
#else
    localtime_r(&now, &local_tm);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string FileRepository::BuildRecordId(const char* prefix, std::size_t next_index) {
    std::ostringstream stream;
    stream << prefix << CurrentDateTimeText();
    std::string text = stream.str();
    std::replace(text.begin(), text.end(), '-', '_');
    std::replace(text.begin(), text.end(), ' ', '_');
    std::replace(text.begin(), text.end(), ':', '_');

    std::ostringstream result;
    result << text << '_' << std::setw(4) << std::setfill('0') << next_index;
    return result.str();
}

bool FileRepository::NeedsAlert(const std::string& alert_level) {
    return alert_level == "warning" || alert_level == "critical";
}

}  // namespace gp::backend


