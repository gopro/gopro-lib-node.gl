#include <cstdio>
#include <Python.h>
#include <vector>
#include <string>
#include <locale>
#include <codecvt>
#ifdef _WIN32
#define setenv(name, value, p) _putenv_s(name, value)
#define NGL_TEST L"../venv/Scripts/ngl-test-script.py"
#include <direct.h>
#else
#define NGL_TEST L"../venv/bin/ngl-test"
#endif
using namespace std;

void py_cmd(const vector<wstring> &args) {
    vector<wchar_t *> wargs(args.size());
    for (uint32_t j = 0; j<args.size(); j++)
        wargs[j] = (wchar_t *)args[j].data();
    Py_Initialize();
    Py_Main(wargs.size(), wargs.data());
    Py_Finalize();
}

wstring to_wstring(const std::string& str) {
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "ERROR: usage run_test <backend> <test>\n"
                "Example: run_test ngfx blending_all_diamond\n");
        return 1;
    }
    chdir(TESTS_DIR);
#ifdef _WIN32
    setenv("PYTHONPATH", "..\\pynodegl;..\\pynodegl-utils", 1);
#else
    setenv("PYTHONPATH", "../pynodegl:../pynodegl-utils:../venv/lib/python3.9/site-packages", 1);
#endif
    setenv("BACKEND", argv[1], 1);
    wstring test_name = to_wstring(argv[2]);
    wstring test_file = test_name.substr(0, test_name.find(L'_')) + L".py";
    vector<wstring> wargs = { to_wstring(argv[0]), NGL_TEST, test_file, test_name }; 
    if (test_file != L"api.py") wargs.push_back(L"refs/" + test_name + L".ref");
    py_cmd(wargs);
    return 0;
}
