#include <jwt-cpp/jwt.h>
#include <iostream>
#include <string>

int main()
{
    std::string token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXUyJ9.eyJleHAiOjE3NDY1MjM2NzcsImlzcyI6ImV4YW1wbGUuY29tIiwicm9sZSI6ImFkbWluIiwidXNlcm5hbWUiOiJBbGljZSJ9.1yKRP3cjVib3oxUtOWyhup9FQJqrod02Crg8lFsib5s";
    try
    {
        // 1. 解码 token
        auto decoded = jwt::decode(token);
        // 2. 验证签名（关键步骤！）
        // auto verifier = jwt::verify()
        //                     .with_issuer("example.com")
        //                     .with_claim("sample", jwt::claim(std::string("test")))
        //                     .allow_algorithm(jwt::algorithm::hs256{"secret"});
        // verifier.verify(decoded);
        //@info jwt-cpp会自动校验时间
        jwt::verify().allow_algorithm(jwt::algorithm::hs256{"secret"}).verify(decoded);
        // 3. 安全访问 payload
        std::cout << "Valid token! Payload:\n";
        for (auto &e : decoded.get_payload_json())
        {
            std::cout << e.first << " = " << e.second << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}