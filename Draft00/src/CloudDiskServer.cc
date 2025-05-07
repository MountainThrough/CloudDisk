#include "../include/CloudDiskServer.h"
#include "../include/Hash.h"
#include "../include/EncryptPassword.h"
#include "../include/SafePreparedStatement.h"
#include "../include/signup.srpc.h"
#include "../include/signup.pb.h"
#include <workflow/MySQLResult.h>
#include <wfrest/json.hpp>
#include <sys/stat.h> // for mkdir
#include <fstream>
#include <jwt-cpp/jwt.h>
using std::map;
using std::ofstream;
using std::string;
using std::vector;
// 读取配置文件
#include "../include/Configuration.h"
Configuration &conf = Configuration::getInstance();
// -llog4cpp -lpthread
#include "../include/Mylogger.h"
CloudDiskServer::CloudDiskServer()
    : http_server(), wait_group(1)
{
}
void CloudDiskServer::start(unsigned short port)
{
    // 当客户端访问服务器时，希望看到客户端请求的信息记录 - track()
    // track() 的返回值是HttpServer的引用
    if (http_server.track().start(port) == 0)
    {
        LogInfo("CloudDiskServer start success!");
        loadModules(); // 注册接口
        // 当服务器启动时，希望看到已经部署好的接口信息 - POST、GET -> list_routes()
        http_server.list_routes();
        wait_group.wait();
    }
    else
    {
        LogError("CloudDiskServer start failed!");
    }
}
// 注册接口
void CloudDiskServer::loadModules()
{
    loadStaticResources();
    loadSignUpModule();
    loadSignInModule();
    //@remind 浏览器会因为是http不让你post数据，所以可以用代理软件burpsuite发送数据给虚拟机，这样浏览器就能post了
}
// 加载静态资源
void CloudDiskServer::loadStaticResources()
{
    http_server.GET("/user/signup", [](const HttpReq *, HttpResp *resp)
                    { resp->File(conf["signup_html"]); });
    http_server.GET("/user/signin", [](const HttpReq *, HttpResp *resp)
                    { resp->File(conf["signin_html"]); });
    http_server.GET("/user/home.html", [](const HttpReq *, HttpResp *resp)
                    { resp->File(conf["home_html"]); });
    http_server.GET("/static/js/auth.js", [](const HttpReq *, HttpResp *resp)
                    { resp->File(conf["auth_js"]); });
    http_server.GET("/static/img/avatar.jpeg", [](const HttpReq *, HttpResp *resp)
                    { resp->File(conf["avatar_jpeg"]); });
    http_server.GET("/file/upload", [](const HttpReq *, HttpResp *resp)
                    { resp->File(conf["index_html"]); });
    http_server.GET("/file/upload/success", [](const HttpReq *, HttpResp *resp)
                    { resp->File(conf["upload_success_html"]); });
}
// 注册
void CloudDiskServer::loadSignUpModule()
{
    //@remind 前端页面浏览器有时候就是不发post请求，抓包也没看到包，postman测试可以发出请求，反正就是莫名奇妙的
    http_server.POST("/user/signup", [](const HttpReq *req, HttpResp *resp, SeriesWork *series)
                     {
        if (req->content_type() == APPLICATION_URLENCODED) {
            // 1. 解析请求
            map<string, string>& form_kv = req->form_kv();
            //@incomplete 用户名和密码都不应该明文传输，可以前端加密和后端解密
            string username = form_kv["username"];
            string password = form_kv["password"];
            // 2. 从注册中心获取ip和port
            const string consul_url = conf["consul_url"];
            // 和注册中心进行交互使用的是HTTP协议，所以需要构建一个HTTPTask
            //@info lambda表达式
            auto consul_task = WFTaskFactory::create_http_task(consul_url, 0, 0, [username, password, resp, series](WFHttpTask* http_task) {
                //@log 这里我要确定是否是只返回一个json数据，因为可能有多个服务，需要确定返回的这个Json数据是我的数据
                //@incomplete 返回来的所有服务的ip和端口,可以再试试只返回指定的ip和端口
                protocol::HttpResponse* consul_resp = http_task->get_resp();
                const void* body;
                size_t sz = 0;
                consul_resp->get_parsed_body(&body, &sz);
                std::string raw_response((const char*)body, sz);
                std::cout << "Raw response from Consul:\n" << raw_response << std::endl;
                using Json = nlohmann::json;
                Json service_info = Json::parse((const char*)body);
                const string ip = service_info["SignupService"]["Address"];
                const unsigned short port = service_info["SignupService"]["Port"];
                LogInfo("SignupService ip = %s, port = %s", ip.c_str(), std::to_string(port).c_str());
                // 3. 找到了后端SignupServer，现在转发给后端，发送RPC请求
	            GOOGLE_PROTOBUF_VERIFY_VERSION;
                // UserService 变成了一个命名空间
	            SignupService::SRPCClient client(ip.c_str(), port);
                // 组装一个ReqSignup消息
	            ReqSignup signup_req;
                signup_req.set_username(username);
                signup_req.set_password(password);
                // 3. 创建RPC任务，并将其加入到序列中运行
                auto rpc_task = client.create_Signup_task([resp](RespSignup* response, srpc::RPCContext* ctx){
                    if (ctx->success() && response->code() == 0) {
                        // 5. 响应
                        // 成功必须返回SUCCESS，前端页面已经写好了
                        resp->String("SUCCESS");
                    }
                    else {
                        resp->String("FAILED");
                    }
                });
                // 序列化消息
                rpc_task->serialize_input(&signup_req);
                // 交给序列运行
                series->push_back(rpc_task);
            });
            series->push_back(consul_task);
        }
        else {
            resp->String("FAILED");
        } });
}
//@info 登录
void CloudDiskServer::loadSignInModule()
{
    http_server.POST("/user/signin", [](const HttpReq *req, HttpResp *resp, SeriesWork *series)
                     {
        if (req->content_type() == APPLICATION_URLENCODED)
        {
            map<string, string> &form_kv = req->form_kv();
            //@info urlendcoded会对特殊字符进行url加密，1234' or 1=1#'转换为"1234%27%20or%201%3D1%23%27"
            string username = form_kv["username"];
            string password = form_kv["password"];
            WFMySQLConnection *conn = new WFMySQLConnection(1);
            const string mysql_url = conf["mysql_url"];
            int ret = conn->init(mysql_url);
            SafePreparedStatement stmt(conn, "SELECT id, salt, passwd FROM CloudDisk.User WHERE username = ? LIMIT 1", "stmt2", series);
            stmt.setString(1, username);
            stmt.execute([resp, password, username, mysql_url, series](WFMySQLTask *mysql_task)
                         {
                // 对MySQL任务进行状态检测
                int state = mysql_task->get_state();
                int error = mysql_task->get_error();
                if (state != WFT_STATE_SUCCESS) {
                    string error_str = WFGlobal::get_error_string(state, error);
                    LogError("%s", error_str.c_str());
                    resp->Error(error, error_str);
                    return;
                }
                // 对SQL进行语法检测
                protocol::MySQLResponse* mysql_resp = mysql_task->get_resp();
                if (mysql_resp->get_packet_type() == MYSQL_PACKET_ERROR) {
                    int error_code = mysql_resp->get_error_code();
                    string error_msg = mysql_resp->get_error_msg();
                    LogError("ERROR %d: %s", error_code, error_msg.c_str());
                    resp->Error(error_code, error_msg);
                    return;
                }
                protocol::MySQLResultCursor cursor(mysql_resp);
                vector<vector<protocol::MySQLCell>> rows;
                //@remind MYSQL_STATUS_GET_RESULT这里没有结果也会进入if语句，只是代表语句执行成功
                if (cursor.get_cursor_status() == MYSQL_STATUS_GET_RESULT && (cursor.fetch_all(rows), !rows.empty()))
                {
                    cursor.fetch_all(rows);
                    string rows_size = std::to_string(rows.size());
                    string suffix = rows.size() > 1 ? " rows in set." : " row in set";
                    string res = rows_size + suffix;
                    LogInfo("%s", res.c_str());
                    //@log 这段要研究token生成
                    //@todo 试试jwt
                    if (rows[0][0].is_ulonglong() && rows[0][1].is_string() && rows[0][2].is_string())
                    {
                        //@info 这里注意user_id是bigInt
                        //@info qq的用户名可以重复是因为那不是个唯一键，唯一键是qq号，那么我这里的唯一键就是用户名,用户id
                        uint64_t user_id = rows[0][0].as_ulonglong();
                        string salt = rows[0][1].as_string();
                        string db_pwd = rows[0][2].as_string();          // 数据库中存储的对应用户的加密密码
                        string in_pwd = encryptPassword(password, salt); // 加密用户登录输入的密码
                        if (db_pwd == in_pwd)
                        { // 登录成功
                            //@info 这个user_id转换为string更好一点，不然不同类型int不兼容
                            string gen_token = jwt::create()
                                                   .set_issuer("example.com")
                                                   .set_type("JWS")
                                                   .set_payload_claim("username", jwt::claim(username))
                                                   .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds(600))
                                                   .sign(jwt::algorithm::hs256{"Mountain"});
                            const string redis_url = conf["redis_url"];
                            auto redis_task = WFTaskFactory::create_redis_task(redis_url, 1, nullptr);
                            //@incomplete 这种后续都要做执行是否成功判断
                            redis_task->get_req()->set_request("SET", {username, gen_token});
                            string cmd = "SET " + username + " " + gen_token;
                            LogInfo("%s", cmd.c_str());
                            series->push_back(redis_task);
                            //@log 不想再用mysql存储token了
                            // 响应
                            // 登录成功的话 前端页面需要服务器返回一个json
                            using Json = nlohmann::json;
                            Json resp_json;
                            Json data;
                            data["Token"] = gen_token;
                            data["Username"] = username;
                            data["Location"] = "/static/view/home.html"; // 注意：这里返回的是route，而不是服务器中home.html的存放路径
                            resp_json["data"] = data;
                            /* resp->Json(resp_json); */
                            // 不能直接发送一个json，前端解析不出来，只能发送一个string
                            resp->String(resp_json.dump());
                            LogInfo(resp_json.dump().c_str());
                        }
                        else
                        {
                            resp->String("FAILED");
                        }
                    }
                    else
                    {
                        resp->String("FAILED");
                    }
                }
                else
                {
                    resp->String("FAILED");
                } 
        });
        } });
}