#include "signin.srpc.h"
#include "workflow/WFFacilities.h"
#include "../include/Mylogger.h"	  // 日志
#include "../include/Configuration.h" // 读取配置文件
Configuration &conf = Configuration::getInstance();
#include <ppconsul/ppconsul.h>
#include <workflow/MySQLResult.h>
#include <workflow/WFFacilities.h>
#include <csignal>
using namespace srpc;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
	wait_group.done();
}

class SigninServiceServiceImpl : public SigninService::Service
{
public:
	void Signin(ReqSignin *request, RespSignin *response, srpc::RPCContext *ctx) override
	{
		// TODO: fill server logic here
		std::string username = request->username();
		std::string password = request->password();
		cout << "username: " << username << " password:" << password << endl;
		response->set_code(0);
		response->set_msg("OK");
	}
};
void timer_callback(WFTimerTask *timer_task)
{
	cout << "SigninService still alive!" << endl;
	using namespace ppconsul::agent;
	Agent *agent = static_cast<Agent *>(timer_task->user_data);
	if (agent)
	{
		agent->servicePass("SigninService");
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
	unsigned short port = 1415;
	SRPCServer server;
	signal(SIGINT, sig_handler);
	SigninServiceServiceImpl signinservice_impl;
	server.add_service(&signinservice_impl);
	server.start(port);
	// 向Consul注册服务
	using namespace ppconsul;
	// dc data center 数据中心
	Consul consul("127.0.0.1:8500", ppconsul::kw::dc = "dc");
	agent::Agent agent(consul);
	agent.registerService(
		agent::kw::name = "SigninService",
		agent::kw::address = "127.0.0.1",
		agent::kw::id = "SigninService",
		agent::kw::port = 1415,
		// 设置心跳检查周期为10s
		agent::kw::check = agent::TtlCheck(std::chrono::seconds(10)));
	agent.servicePass("SigninService");
	auto timer_task = WFTaskFactory::create_timer_task(9 * 1000 * 1000, timer_callback);
	timer_task->user_data = &agent;
	server.start(port);
	timer_task->start();
	wait_group.wait();
	server.stop();
	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}
