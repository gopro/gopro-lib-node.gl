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

#pragma once
#include <ctime>
#include <string>
#include <map>
#include <regex>
#include <vector>
#include <json.hpp>
using json = nlohmann::json;

namespace ngfx {
class ShaderTools {
public:
    ShaderTools();
    std::vector<std::string> compileShaders(const std::vector<std::string>& files, const std::string& defines, std::string outDir, std::string fmt = "glsl");
    std::vector<std::string> convertShaders(const std::vector<std::string> &files, std::string outDir, std::string fmt);
    std::vector<std::string> generateShaderMaps(const std::vector<std::string>& files, std::string outDir, std::string fmt);
private:
    void applyPatches(const std::vector<std::string> &patchFiles, std::string outDir);
    int cmd(std::string str);
    int compileShaderGLSL(std::string filename, const std::string& defines, const std::string& outDir,
                          std::vector<std::string>& outFiles);
    int compileShaderHLSL(const std::string& file, const std::string& defines, std::string outDir, std::vector<std::string>& outFiles);
    int compileShaderMTL(const std::string& file, const std::string& defines, std::string outDir, std::vector<std::string> &outFiles);
    int convertShader(const std::string& file, const std::string& extraArgs, std::string outDir, std::string fmt, std::vector<std::string>& outFiles);
    bool findIncludeFile(const std::string& includeFilename, const std::vector<std::string> &includePaths,
        std::string& includeFile);
    struct MetalReflectData {
        std::vector<std::smatch> attributes, buffers, textures;
    };
    bool findMetalReflectData(const std::vector<std::smatch>& metalReflectData, const std::string& name, std::smatch &match);
    void generateShaderMapMSL(const std::string& file, std::string outDir, std::vector<std::string>& outFiles);
    int genShaderReflectionGLSL(const std::string& file, std::string outDir);
    json patchShaderReflectionDataMSL(const std::string& file, json& reflectData, const std::string& ext);
    std::string preprocess(const std::string& dataPath, const std::string& inFile);
    bool verbose = false;
    std::vector<std::string> defaultIncludePaths;
};
};