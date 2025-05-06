#include "../include/signup.srpc.h"
#include "../include/EncryptPassword.h"       // 密码加密
#include "../include/Mylogger.h"              // 日志
#include "../include/Configuration.h"         // 读取配置文件
#include "../include/SafePreparedStatement.h" // 防止sql注入文件
Configuration &conf = Configuration::getInstance();
#include <ppconsul/ppconsul.h>
#include <workflow/MySQLResult.h>
#include <workflow/WFFacilities.h>
#include <workflow/WFMySQLConnection.h>
#include <csignal>
using namespace srpc;
static WFFacilities::WaitGroup wait_group(1);
void sig_handler(int signo)
{
    wait_group.done();
}
class SignupServiceServiceImpl : public SignupService::Service
{
public:
    void Signup(ReqSignup *request, RespSignup *response, srpc::RPCContext *ctx) override
    {
        std::string username = request->username();
        std::string password = request->password();
        LogInfo("username: %s password: %s", username.c_str(), password.c_str());
        string salt = generateSalt();
        string encrypted_pwd = encryptPassword(password, salt);
        cout << "salt: " << salt << endl;
        cout << "encrypted_pwd: " << encrypted_pwd << endl;
        LogInfo("salt: %s encrypted_pwd: %s", salt.c_str(), encrypted_pwd.c_str());
        //@log 解决sql注入
        WFMySQLConnection *conn = new WFMySQLConnection(0);
        int ret = conn->init(conf["mysql_url"]);
        SafePreparedStatement stmt(conn, "INSERT INTO CloudDisk.User (username, salt, passwd) VALUES (?, ?, ?)", "stmt1", ctx->get_series());
        stmt.setString(1, username);
        stmt.setString(2, salt);
        stmt.setString(3, encrypted_pwd);
        stmt.execute([response, ctx](WFMySQLTask *task)
                     {
            int state = task->get_state();
            int error = task->get_error();
            if (state != WFT_STATE_SUCCESS)
            {
                string error_str = WFGlobal::get_error_string(state, error);
                LogError("%s", error_str.c_str());
                response->set_code(-1);
                response->set_msg("Signup Failed");
                return;
            }
            protocol::MySQLResponse *mysql_resp = task->get_resp();
            if (mysql_resp->get_packet_type() == MYSQL_PACKET_ERROR)
            {
                int error_code = mysql_resp->get_error_code();
                string error_msg = mysql_resp->get_error_msg();
                LogError("ERROR %d: %s", error_code, error_msg.c_str());
                response->set_code(-1);
                response->set_msg("Signup Failed");
                return;
            }
            protocol::MySQLResultCursor cursor(mysql_resp);
            //@remind 这里不需要判断是否有结果，因为本身就没有结果，只是看影响了几行
            if (cursor.get_cursor_status() == MYSQL_STATUS_GET_RESULT)
            {
                int affected_rows = cursor.get_affected_rows();
                string suffix = affected_rows > 1 ? " rows affected" : " row affected";
                string res = std::to_string(affected_rows) + suffix;
                LogInfo("Query OK, %s", res.c_str());
                response->set_code(0);
                response->set_msg("OK");
            }
            else
            {
                response->set_code(-1);
                response->set_msg("Signup Failed");
            } });
    }
};
void timer_callback(WFTimerTask *timer_task)
{
    // cout << "SignupService still alive!" << endl;
    using namespace ppconsul::agent;
    Agent *agent = static_cast<Agent *>(timer_task->user_data);
    if (agent)
    {
        agent->servicePass("SignupService");
    }
    else
    {
        cout << "nullptr" << endl;
    }
    auto next_task = WFTaskFactory::create_timer_task(9 * 1000 * 1000, timer_callback);
    next_task->user_data = agent;
    series_of(timer_task)->push_back(next_task);
}
int main()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    unsigned short port = 1412;
    SRPCServer server;
    signal(SIGINT, sig_handler);
    SignupServiceServiceImpl signupservice_impl;
    server.add_service(&signupservice_impl);
    server.start(port);
    // 向Consul注册服务
    using namespace ppconsul;
    // dc data center 数据中心
    Consul consul("127.0.0.1:8500", ppconsul::kw::dc = "dc");
    agent::Agent agent(consul);
    agent.registerService(
        agent::kw::name = "SignupService",
        agent::kw::address = "127.0.0.1",
        agent::kw::id = "SignupService",
        agent::kw::port = 1412,
        // 设置心跳检查周期为10s
        agent::kw::check = agent::TtlCheck(std::chrono::seconds(10)));
    /* agent.deregisterService("SignupService"); // 取消注册 */
    // 每5s发送一次心跳 - 告诉注册中心我还活着
    agent.servicePass("SignupService");
    // 这里不能使用Lambda表达式，需要循环创建定时器任务
    //@info 这里不用lambda表达式是因为有嵌套
    auto timer_task = WFTaskFactory::create_timer_task(9 * 1000 * 1000, timer_callback);
    timer_task->user_data = &agent;
    timer_task->start();
    wait_group.wait();
    server.stop();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
