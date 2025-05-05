#pragma once
#include <string>
#include <vector>
#include <functional>
#include <workflow/WFMySQLConnection.h>
#include <workflow/MySQLUtil.h>
#include <workflow/MySQLResult.h>
#include <workflow/Workflow.h>
#include <algorithm>
class SafePreparedStatement
{
public:
    SafePreparedStatement(WFMySQLConnection *conn,
                          const std::string &sql,
                          const std::string &stmt_name);
    ~SafePreparedStatement();

    SafePreparedStatement &setString(unsigned int index, const std::string &value);
    SafePreparedStatement &setInt(unsigned int index, int value);
    SafePreparedStatement &setUInt(unsigned int index, unsigned int value);
    SafePreparedStatement &setInt64(unsigned int index, int64_t value);
    SafePreparedStatement &setUInt64(unsigned int index, uint64_t value);
    SafePreparedStatement &setDouble(unsigned int index, double value);
    SafePreparedStatement &setBoolean(unsigned int index, bool value);
    SafePreparedStatement &setNull(unsigned int index);

    void execute(std::function<void(WFMySQLTask *)> callback);

private:
    WFMySQLConnection *conn_;
    std::string stmt_name_;
    std::string sql_;
    std::vector<std::string> params_;
    SeriesWork *series_;
    bool dealloc_added_ = false;

    std::string buildExecuteSQL() const;
    void append_dealloc_task();
};