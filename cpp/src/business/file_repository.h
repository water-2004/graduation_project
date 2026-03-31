#pragma once

#include "business/business_types.h"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace gp::backend {

// 业务仓库：当前版本基于 SQLite 持久化，并兼容首次导入旧版 TSV 数据。
class FileRepository {
public:
    explicit FileRepository(std::filesystem::path data_dir);
    ~FileRepository();

    void Initialize();

    bool ValidateLogin(const std::string& username, const std::string& password, User* out_user) const;
    std::vector<Patient> ListPatients() const;
    std::optional<Patient> GetPatientById(const std::string& patient_id) const;
    bool AddPatient(const Patient& patient, std::string* err);
    bool UpdatePatient(const Patient& patient, std::string* err);
    bool DeletePatient(const std::string& patient_id, std::string* err);

    bool AddMonitorRecord(const MonitorRecord& record, std::string* err, bool* alert_created = nullptr);
    std::vector<MonitorRecord> ListMonitorRecords(const std::string& patient_id) const;
    std::vector<MonitorRecord> ListAllMonitorRecords() const;
    std::vector<AlertRecord> ListAlerts(const std::string& patient_id) const;
    std::vector<AlertRecord> ListAllAlerts() const;
    bool ConfirmAlert(const std::string& alert_id, const std::string& confirmed_by, std::string* err);
    bool ChangePassword(const std::string& username, const std::string& old_password,
                        const std::string& new_password, std::string* err);

private:
    void OpenDatabase();
    void CreateSchema();
    void SeedDefaultUserIfNeeded();
    void ImportLegacyTsvIfNeeded();
    bool TableIsEmpty(const char* table_name) const;
    std::size_t CountRows(const char* table_name) const;
    bool PatientExists(const std::string& patient_id) const;
    void Execute(const char* sql) const;

    static std::vector<std::string> SplitTsvLine(const std::string& line);
    static std::string SanitizeField(const std::string& value);
    static std::string CurrentDateTimeText();
    static std::string BuildRecordId(const char* prefix, std::size_t next_index);
    static bool NeedsAlert(const std::string& alert_level);

    std::filesystem::path data_dir_;
    std::filesystem::path db_file_;
    std::filesystem::path users_file_;
    std::filesystem::path patients_file_;
    std::filesystem::path monitor_records_file_;
    std::filesystem::path alerts_file_;

    mutable std::mutex mutex_;
    sqlite3* db_ = nullptr;
};

}  // namespace gp::backend

