#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <unistd.h>
#include "../include/SafePreparedStatement.h"
using namespace protocol;
// 上下文结构体
struct PreparedStatementContext
{
    std::string stmt_name;
    std::string sql;
    std::vector<std::string> params;
    std::function<void(WFMySQLTask *)> user_callback;
};
// 构造函数：创建 PREPARE 任务并加入序列
SafePreparedStatement::SafePreparedStatement(WFMySQLConnection *conn,
                                             const std::string &sql,
                                             const std::string &stmt_name, SeriesWork *series)
    : conn_(conn), stmt_name_(stmt_name), sql_(sql), series_(series)
{
    if (!conn_)
    {
        throw std::invalid_argument("Connection cannot be null");
    }
    std::string escaped_sql = MySQLUtil::escape_string(sql_);
    params_.resize(std::count(escaped_sql.begin(), escaped_sql.end(), '?'));
    std::string prepare_sql = "PREPARE " + stmt_name_ + " FROM '" + escaped_sql + "';";
    auto prepare_task = conn_->create_query_task(prepare_sql, [this](WFMySQLTask *task)
                                                 {
        if (task->get_state() != WFT_STATE_SUCCESS) {
            MySQLResponse* resp = task->get_resp();
            if (resp->get_packet_type() == MYSQL_PACKET_ERROR) {
                std::cerr << "Prepare failed: " << resp->get_error_msg() << std::endl;
            }
        } });
    series_->push_back(prepare_task);
}
// 析构函数：添加 DEALLOCATE 任务并启动序列
SafePreparedStatement::~SafePreparedStatement()
{
    if (series_ && !dealloc_added_)
    {
        append_dealloc_task();
    }
}
// 构建 EXECUTE SQL
std::string SafePreparedStatement::buildExecuteSQL() const
{
    std::ostringstream oss;
    oss << "EXECUTE " << stmt_name_ << " USING ";
    for (size_t i = 0; i < params_.size(); ++i)
    {
        if (i > 0)
            oss << ", ";
        oss << params_[i];
    }
    oss << ";";
    return oss.str();
}
// 添加 DEALLOCATE 任务到序列
void SafePreparedStatement::append_dealloc_task()
{
    std::string dealloc_sql = "DEALLOCATE PREPARE " + stmt_name_ + ";";
    std::cout << "xigou" << std::endl;
    auto dealloc_task = conn_->create_query_task(dealloc_sql, [](WFMySQLTask *task)
                                                 {
        if (task->get_state() != WFT_STATE_SUCCESS) {
            MySQLResponse* resp = task->get_resp();
            if (resp->get_packet_type() == MYSQL_PACKET_ERROR) {
                std::cerr << "Deallocate failed: " << resp->get_error_msg() << std::endl;
            }
        } });
    //@remind 这里应该没有啥问题，因为我看wfrest也是直接往正在运行的序列添加任务
    series_->push_back(dealloc_task);
    dealloc_added_ = true;
}
// 设置参数接口（同步）
SafePreparedStatement &SafePreparedStatement::setString(unsigned int index, const std::string &value)
{
    if (index == 0 || index > params_.size() + 1)
    {
        throw std::out_of_range("Parameter index out of range");
    }
    // 设置为用户变量名
    std::string var_name = "@param_" + std::to_string(index - 1);
    std::string escaped = protocol::MySQLUtil::escape_string(value);
    // 构造 SET @param_0 = 'Alice' 的语句
    std::string set_sql = "SET " + var_name + " = '" + escaped + "';";
    // 创建设置变量的任务，并加入序列
    auto set_task = conn_->create_query_task(set_sql, [](WFMySQLTask *task)
                                             {
        if (task->get_state() != WFT_STATE_SUCCESS)
        {
            std::cerr << "Failed to set parameter variable" << std::endl;
        } });
    series_->push_back(set_task);  // 加入任务链
    params_[index - 1] = var_name; // 存储变量名，用于 EXECUTE
    return *this;
}
SafePreparedStatement &SafePreparedStatement::setInt(unsigned int index, int value)
{
    return setString(index, std::to_string(value));
}
SafePreparedStatement &SafePreparedStatement::setUInt(unsigned int index, unsigned int value)
{
    return setString(index, std::to_string(value));
}
SafePreparedStatement &SafePreparedStatement::setInt64(unsigned int index, int64_t value)
{
    return setString(index, std::to_string(value));
}
SafePreparedStatement &SafePreparedStatement::setUInt64(unsigned int index, uint64_t value)
{
    return setString(index, std::to_string(value));
}
SafePreparedStatement &SafePreparedStatement::setDouble(unsigned int index, double value)
{
    return setString(index, std::to_string(value));
}
SafePreparedStatement &SafePreparedStatement::setBoolean(unsigned int index, bool value)
{
    return setString(index, value ? "1" : "0");
}
SafePreparedStatement &SafePreparedStatement::setNull(unsigned int index)
{
    return setString(index, "NULL");
}
// 执行接口
void SafePreparedStatement::execute(std::function<void(WFMySQLTask *)> callback)
{
    std::string string1 = buildExecuteSQL();
    auto execute_task = conn_->create_query_task(buildExecuteSQL(), callback);
    series_->push_back(execute_task);
}