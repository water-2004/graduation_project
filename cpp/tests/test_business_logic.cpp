#include "business/business_logic.h"

#include <filesystem>
#include <gtest/gtest.h>

namespace fs = std::filesystem;
using gp::backend::BusinessLogic;
using gp::backend::FileRepository;

class BusinessLogicTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = fs::temp_directory_path() / "gp_test_business";
        fs::create_directories(temp_dir_);
        repo_ = std::make_shared<FileRepository>(temp_dir_);
        repo_->Initialize();
        logic_ = std::make_unique<BusinessLogic>(repo_);
    }

    void TearDown() override {
        logic_.reset();
        repo_.reset();
        fs::remove_all(temp_dir_);
    }

    std::string Send(const std::string& line) {
        bool close = false;
        return logic_->HandleLine(line, &close);
    }

    bool SendAndCheckClose(const std::string& line) {
        bool close = false;
        logic_->HandleLine(line, &close);
        return close;
    }

    fs::path temp_dir_;
    std::shared_ptr<FileRepository> repo_;
    std::unique_ptr<BusinessLogic> logic_;
};

TEST_F(BusinessLogicTest, Ping) {
    EXPECT_EQ(Send("PING"), "PONG");
}

TEST_F(BusinessLogicTest, PingCaseInsensitive) {
    EXPECT_EQ(Send("ping"), "PONG");
    EXPECT_EQ(Send("Ping"), "PONG");
}

TEST_F(BusinessLogicTest, Quit) {
    EXPECT_TRUE(SendAndCheckClose("QUIT"));
}

TEST_F(BusinessLogicTest, EmptyCommand) {
    EXPECT_TRUE(Send("").find("ERR") != std::string::npos);
}

TEST_F(BusinessLogicTest, UnknownCommand) {
    EXPECT_TRUE(Send("FOOBAR").find("ERR") != std::string::npos);
}

TEST_F(BusinessLogicTest, LoginWithDefaultUser) {
    const std::string result = Send("LOGIN admin 123456");
    EXPECT_TRUE(result.find("LOGIN_OK") != std::string::npos);
    EXPECT_TRUE(result.find("role=admin") != std::string::npos);
}

TEST_F(BusinessLogicTest, LoginWrongPassword) {
    const std::string result = Send("LOGIN admin wrongpass");
    EXPECT_TRUE(result.find("ERR") != std::string::npos);
}

TEST_F(BusinessLogicTest, LoginMissingFields) {
    EXPECT_TRUE(Send("LOGIN").find("ERR") != std::string::npos);
    EXPECT_TRUE(Send("LOGIN admin").find("ERR") != std::string::npos);
}

TEST_F(BusinessLogicTest, AddAndListPatients) {
    const std::string add_result = Send("ADD_PATIENT P001|TestPatient|M|30|1234567890|remark");
    EXPECT_TRUE(add_result.find("OK") != std::string::npos);

    const std::string list_result = Send("LIST_PATIENTS");
    EXPECT_TRUE(list_result.find("PATIENTS") != std::string::npos);
    EXPECT_TRUE(list_result.find("P001") != std::string::npos);
    EXPECT_TRUE(list_result.find("TestPatient") != std::string::npos);
}

TEST_F(BusinessLogicTest, GetPatient) {
    Send("ADD_PATIENT P002|John|M|45|9876543210|notes");
    const std::string result = Send("GET_PATIENT P002");
    EXPECT_TRUE(result.find("PATIENT") != std::string::npos);
    EXPECT_TRUE(result.find("John") != std::string::npos);
}

TEST_F(BusinessLogicTest, GetPatientNotFound) {
    EXPECT_TRUE(Send("GET_PATIENT NONEXIST").find("ERR") != std::string::npos);
}

TEST_F(BusinessLogicTest, UpdatePatient) {
    Send("ADD_PATIENT P003|OldName|F|50|111|remark");
    const std::string result = Send("UPDATE_PATIENT P003|NewName|F|51|222|new_remark");
    EXPECT_TRUE(result.find("OK") != std::string::npos);

    const std::string get_result = Send("GET_PATIENT P003");
    EXPECT_TRUE(get_result.find("NewName") != std::string::npos);
}

TEST_F(BusinessLogicTest, DeletePatient) {
    Send("ADD_PATIENT P004|ToDelete|M|40|333|del");
    EXPECT_TRUE(Send("DELETE_PATIENT P004").find("OK") != std::string::npos);
    EXPECT_TRUE(Send("GET_PATIENT P004").find("ERR") != std::string::npos);
}

TEST_F(BusinessLogicTest, AddMonitorRecordAndList) {
    Send("ADD_PATIENT P005|RecordTest|M|60|444|rec");
    const std::string add_result = Send(
        "ADD_MONITOR_RECORD P005|N|0.95|normal|12.3|manual|sample1|N");
    EXPECT_TRUE(add_result.find("RECORD_OK") != std::string::npos);

    const std::string list_result = Send("LIST_MONITOR_RECORDS P005");
    EXPECT_TRUE(list_result.find("MONITOR_RECORDS") != std::string::npos);
    EXPECT_TRUE(list_result.find("P005") != std::string::npos);
}

TEST_F(BusinessLogicTest, AddMonitorRecordWithAlert) {
    Send("ADD_PATIENT P006|AlertTest|M|70|555|alert");
    const std::string result = Send(
        "ADD_MONITOR_RECORD P006|V|0.88|critical|15.2|sample|beat1|V");
    EXPECT_TRUE(result.find("RECORD_OK") != std::string::npos);
    EXPECT_TRUE(result.find("报警") != std::string::npos);

    const std::string alerts = Send("LIST_ALERTS P006");
    EXPECT_TRUE(alerts.find("ALERTS") != std::string::npos);
    EXPECT_TRUE(alerts.find("critical") != std::string::npos);
}

TEST_F(BusinessLogicTest, ConfirmAlert) {
    Send("ADD_PATIENT P007|ConfirmTest|F|55|666|conf");
    Send("ADD_MONITOR_RECORD P007|S|0.75|warning|10.1|manual||S");

    const std::string alerts = Send("LIST_ALERTS P007");
    // Extract alert_id from response
    auto pos = alerts.find("ALERTS ");
    ASSERT_NE(pos, std::string::npos);
    std::string alert_data = alerts.substr(pos + 7);
    auto pipe_pos = alert_data.find('|');
    ASSERT_NE(pipe_pos, std::string::npos);
    std::string alert_id = alert_data.substr(0, pipe_pos);

    const std::string confirm_result = Send("CONFIRM_ALERT " + alert_id + "|admin");
    EXPECT_TRUE(confirm_result.find("ALERT_OK") != std::string::npos);
}

TEST_F(BusinessLogicTest, ChangePassword) {
    // Default user is admin/123456
    const std::string result = Send("CHANGE_PASSWORD admin|123456|newpass");
    EXPECT_TRUE(result.find("PASSWORD_OK") != std::string::npos);

    // Login with new password
    EXPECT_TRUE(Send("LOGIN admin newpass").find("LOGIN_OK") != std::string::npos);
    // Old password should fail
    EXPECT_TRUE(Send("LOGIN admin 123456").find("ERR") != std::string::npos);
}

TEST_F(BusinessLogicTest, ChangePasswordWrongOld) {
    EXPECT_TRUE(Send("CHANGE_PASSWORD admin|wrongold|newpass").find("ERR") != std::string::npos);
}
