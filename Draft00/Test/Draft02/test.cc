#include <jwt-cpp/jwt.h>
#include <iostream>
#include <string>

int main()
{
    // std::string token = jwt::create()
    //                         .set_issuer("example.com")
    //                         .set_type("JWS")
    //                         .set_payload_claim("username", jwt::claim(std::string("Alice"))) // 显式转成 string
    //                         .set_payload_claim("role", jwt::claim(std::string("admin")))
    //                         .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds(-15))
    //                         .sign(jwt::algorithm::hs256{"secret"});
    // std::cout << "Token: " << token << std::endl;
    std::string token = jwt::create()
    .set_payload_claim("username", jwt::claim(std::string("Alice"))) // 显式转成 string
    .sign(jwt::algorithm::hs256{"secret"});
std::cout << "Token: " << token << std::endl;
    // eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXUyJ9.eyJleHAiOjE3NDY2MDY2MTYsImlzcyI6ImV4YW1wbGUuY29tIiwicm9sZSI6ImFkbWluIiwidXNlcm5hbWUiOiJBbGljZSJ9.b30QurjZ2qXMjpu4u-4S7ARLVF_YDccAz_BB6SBYRY4
    return 0;
}