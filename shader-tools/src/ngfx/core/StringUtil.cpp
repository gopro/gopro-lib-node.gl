#include "StringUtil.h"
using namespace std;
using namespace ngfx;

string StringUtil::toLower(const string& str) {
    string r = str;
    for (uint32_t j = 0; j<r.size(); j++) r[j] = tolower(r[j]);
    return r;
}
