#pragma once
#include <string>
using std::string;
class Hash
{
public:
    Hash(const string &filename_);
    string sha1() const;

private:
    string filename;
};