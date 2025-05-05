#pragma once
/* v3.0 */
#include <wfrest/HttpServer.h>     // for HttpServer
#include <workflow/WFFacilities.h> // for WaitGroup
using namespace wfrest;
class CloudDiskServer
{
public:
    CloudDiskServer();
    void start(unsigned short port);

private:
    // 注册接口
    void loadModules();
    /* 部署接口 */
    // 加载静态资源
    void loadStaticResources();
    // 注册
    void loadSignUpModule();
    // 登录
    void loadSignInModule();
    // 加载用户信息
    void loadUserInfoModule();
    // 加载用户文件列表
    void loadUserFileListModule();
    // 上传文件
    void loadFileUploadModule();
    // 下载文件
    void loadFileDownloadModule();

private:
    HttpServer http_server;
    WFFacilities::WaitGroup wait_group;
};
