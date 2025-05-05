#include "SafePreparedStatement.h"
#include <workflow/WFFacilities.h>
#include <iostream>
#include <signal.h>
WFFacilities::WaitGroup wait_group(1);
void sig_handler(int signo)
{
    wait_group.done();
}
int main()
{
    signal(SIGINT, sig_handler);
    WFMySQLConnection *conn = new WFMySQLConnection(0); // id=0
    int ret = conn->init("mysql://root:1234@localhost");
    SafePreparedStatement stmt(conn, "INSERT INTO Test.users (username, age) VALUES (?, ?)", "stmt1");
    //@remind 注意sql语句不要直接用字符串拼接，也不要盲目相信数据库中的数据，不然有可能导致二次注入
    stmt.setString(1, "Rober2345t'); DROP TABLE Test.users; --");
    stmt.setInt(2, 25);
    stmt.execute([](WFMySQLTask *task)
                 {
            //@info 这段代码的错误判断不完整，但是可以暂时使用，目前暴露的问题是插入相同主键不报错
        // if (task->get_state() == WFT_STATE_SUCCESS) {
        //     std::cout << "Insert successful!" << std::endl;
        // } else {
        //     std::cerr << "Insert failed." << std::endl;
        // } });
        if (task->get_state() != WFT_STATE_SUCCESS)
        {
            // 网络层失败（比如连接断开、超时）
            std::cerr << "Network error: " << task->get_error() << std::endl;
            return;
        }
        if (task->get_resp()->get_packet_type() == MYSQL_PACKET_OK)
        {
            std::cout << "Insert successful!" << std::endl;
        }
        else if (task->get_resp()->get_packet_type() == MYSQL_PACKET_ERROR)
        {
            std::cerr << "MySQL Error: " << task->get_resp()->get_error_msg() << std::endl;
        }
        else
        {
            std::cerr << "Unexpected packet type: " << (int)task->get_resp()->get_packet_type() << std::endl;
        } });
    wait_group.wait();
    return 0;
}