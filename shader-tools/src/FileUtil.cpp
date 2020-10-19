/*
 * Copyright 2016 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include "FileUtil.h"
#include <fstream>
#include <cstring>
#include <cassert>
#include <chrono>
using namespace std;
namespace fs = std::filesystem;
using namespace ngfx;

bool FileUtil::getmtime(const string& filename, fs::file_time_type &mtime) {
    if (!fs::exists(filename)) { return false; }
    mtime = fs::last_write_time(filename);
    return true;
}

bool FileUtil::srcFileChanged(const string& srcFileName, const string& targetFileName) {
    fs::file_time_type srcTimeStamp, targetTimeStamp;
    getmtime(srcFileName, srcTimeStamp);
    if (!getmtime(targetFileName, targetTimeStamp)) return true;
    if (srcTimeStamp > targetTimeStamp)
        return true;
    return false;
}

string FileUtil::tempDir() {
    return fs::canonical(fs::temp_directory_path()).string();
}

string FileUtil::readFile(const string& path) {
    string contents;
    ifstream in(path, ifstream::binary | ios::ate);
    assert(in);
    auto size = in.tellg();
    in.seekg(0);
    contents.resize(size);
    in.read(&contents[0], size);
    in.close();
    return contents;
}

void FileUtil::writeFile(const string& path, const string& contents) {
    ofstream out(path);
    assert(out);
    out.write(contents.data(), contents.size());
    out.close();
}

vector<string> FileUtil::splitExt(const string& filename) {
    auto it = filename.find_last_of('.');
    return { filename.substr(0, it), filename.substr(it) };
}

vector<string> FileUtil::findFiles(const string& path) {
    vector<string> files;
    for (const auto & entry : fs::directory_iterator(path)) {
        files.push_back(entry.path().string());
    }
    return files;
}

vector<string> FileUtil::findFiles(const string& path, const string& ext) {
    vector<string> files;
    for (const auto & entry : fs::directory_iterator(path)) {
        const fs::path& path = entry.path();
        if (path.extension() != ext) continue;
        files.push_back(path.string());
    }
    return files;
}

vector<string> FileUtil::filterFiles(const vector<string>& files, const string& fileFilter) {
    vector<string> filteredFiles;
    for (const string& file: files) {
      if (strstr(file.c_str(), fileFilter.c_str()))
          filteredFiles.push_back(file);
    }
    return filteredFiles;
}

vector<string> FileUtil::findFiles(const vector<string>& paths, const vector<string>& extensions) {
    vector<string> files;
    for (const string& path: paths) {
        for (const string& ext: extensions) {
            vector<string> filteredFiles = findFiles(path, ext);
            files.insert(files.end(),filteredFiles.begin(), filteredFiles.end());
        }
    }
    return files;
}

