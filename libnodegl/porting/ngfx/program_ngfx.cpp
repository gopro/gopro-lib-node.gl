/*
 * Copyright 2018 GoPro Inc.
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

#include "memory.h"
#include "program_ngfx.h"
#include <stdlib.h>
#include "log.h"
#include "graphics/ShaderModule.h"
#include "graphics/ShaderTools.h"
#include "FileUtil.h"
#include "ProcessUtil.h"
#include "gctx_ngfx.h"
#include <filesystem>
using namespace ngfx;
using namespace std;
namespace fs = std::filesystem;
static ShaderTools shaderTools(true); //false

struct program *ngli_program_ngfx_create(struct gctx *gctx) {
    program_ngfx *s = (program_ngfx*)ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct program *)s;
}

struct ShaderCompiler {
    ~ShaderCompiler() {
        for (const string& path : glslFiles) fs::remove(path);
        for (const string& path : spvFiles) fs::remove(path);
        for (const string& path : spvMapFiles) fs::remove(path);
        //TODO fs::remove(tmpDir);
    }
    string compile(string src, const string& ext) {
        //patch source bindings
        tmpDir = fs::path(FileUtil::tempDir() + "/" + "nodegl" + "/" + to_string(ProcessUtil::getPID())).make_preferred().string();
        fs::create_directories(tmpDir);
        string tmpFile = fs::path(tmpDir + "/" + "tmp" + ext).make_preferred().string();
        FileUtil::writeFile(tmpFile, src);
        string outDir = tmpDir;
        glslFiles = { tmpFile };
        spvFiles = shaderTools.compileShaders(glslFiles, outDir, "glsl", "", ShaderTools::PATCH_SHADER_LAYOUTS_GLSL);
        spvMapFiles = shaderTools.generateShaderMaps(glslFiles, outDir, "glsl");
#if defined(GRAPHICS_BACKEND_DIRECT3D12)
        auto hlslFiles = shaderTools.convertShaders(spvFiles, outDir, "hlsl");
        auto hlsllibFiles = shaderTools.compileShaders(hlslFiles, outDir, "hlsl");
        auto hlslMapFiles = shaderTools.generateShaderMaps(hlslFiles, outDir, "hlsl");
#endif
        return FileUtil::splitExt(spvFiles[0])[0];
    }
    string tmpDir;
    std::vector<string> glslFiles, spvFiles, spvMapFiles;
};

int ngli_program_ngfx_init(struct program *s, const char *vertex, const char *fragment, const char *compute) {
    gctx_ngfx *gctx = (gctx_ngfx  *)s->gctx;
    program_ngfx* program = (program_ngfx*)s;
    if (vertex) {
        ShaderCompiler sc;
        program->vs = VertexShaderModule::create(gctx->graphics_context->device, sc.compile(vertex, ".vert")).release();
    }
    if (fragment) {
        ShaderCompiler sc;
        program->fs = FragmentShaderModule::create(gctx->graphics_context->device, sc.compile(fragment, ".frag")).release();
    }
    if (compute) {
        ShaderCompiler sc;
        program->cs = ComputeShaderModule::create(gctx->graphics_context->device, sc.compile(compute, ".comp")).release();
    }
    return 0;
}
void ngli_program_ngfx_freep(struct program **sp) {
    program_ngfx* program = (program_ngfx*)*sp;
    if (program->vs) { delete program->vs; }
    if (program->fs) { delete program->fs; }
    if (program->cs) { delete program->cs; }
    ngli_freep(sp);
}

