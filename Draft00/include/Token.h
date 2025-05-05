#pragma once
#include <string>
using std::string;

class Token
{
public:
    Token(const string &username_, const string &salt_);

    string generateToken() const;

private:
    string username;
    string salt;
};