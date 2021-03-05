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

#include "program_ngfx.h"
extern "C" {
#include "memory.h"
}
#include <stdlib.h>
#include "ngfx/graphics/ShaderModule.h"
#include "ngfx/graphics/ShaderTools.h"
#include "ngfx/core/FileUtil.h"
#include "ngfx/core/ProcessUtil.h"
#include "ngfx/core/Util.h"
#include "gctx_ngfx.h"
#include <filesystem>
using namespace ngfx;
using namespace std;
namespace fs = std::filesystem;
enum { DEBUG_FLAG_VERBOSE = 1, DEBUG_FLAG_KEEP_INTERMEDIATE_FILES = 2 };
static int DEBUG_FLAGS = DEBUG_FLAG_VERBOSE | DEBUG_FLAG_KEEP_INTERMEDIATE_FILES; // 0;
static ShaderTools shaderTools(DEBUG_FLAGS & DEBUG_FLAG_VERBOSE);

struct program *ngli_program_ngfx_create(struct gctx *gctx) {
    program_ngfx *s = (program_ngfx*)ngli_calloc(1, sizeof(*s));
    if (!s)
        return NULL;
    s->parent.gctx = gctx;
    return (struct program *)s;
}

struct ShaderCompiler {
    ~ShaderCompiler() {
        if (!(DEBUG_FLAGS & DEBUG_FLAG_KEEP_INTERMEDIATE_FILES)) {
            for (const string& path : glslFiles) fs::remove(path);
            for (const string& path : spvFiles) fs::remove(path);
#if defined(NGFX_GRAPHICS_BACKEND_VULKAN)
            for (const string& path : glslMapFiles) fs::remove(path);
#elif defined(NGFX_GRAPHICS_BACKEND_DIRECT3D12)
            for (const string& path : hlslFiles) fs::remove(path);
            for (const string& path : dxcFiles) fs::remove(path);
            for (const string& path : hlslMapFiles) fs::remove(path);
#elif defined(NGFX_GRAPHICS_BACKEND_METAL)
            for (const string& path : mtlFiles) fs::remove(path);
            for (const string& path : mtllibFiles) fs::remove(path);
            for (const string& path : mtlMapFiles) fs::remove(path);
#endif
            //TODO fs::remove(tmpDir);
        }
    }
    string compile(string src, const string& ext) {
        uint64_t h1 = Util::hash(src);
        tmpDir = fs::path(FileUtil::tempDir() + "/" + "nodegl").make_preferred().string();
        fs::create_directories(tmpDir);
        string tmpFile = fs::path(tmpDir + "/" + "tmp_" + to_string(h1) + ext).make_preferred().string();
        FileUtil::Lock lock(tmpFile);
        if (!fs::exists(tmpFile)) {
            FileUtil::writeFile(tmpFile, src);
        }
        //else {
            //if (FileUtil::readFile(tmpFile) != src) {
            //    LOG(ERROR, "hash collision: tmpFile: %s", tmpFile.c_str());
            //}
        //}
        string outDir = tmpDir;
        glslFiles = { tmpFile };
        spvFiles = shaderTools.compileShaders(glslFiles, outDir, ShaderTools::FORMAT_GLSL, {},
            ShaderTools::PATCH_SHADER_LAYOUTS_GLSL | ShaderTools::REMOVE_UNUSED_VARIABLES);
#if defined(NGFX_GRAPHICS_BACKEND_VULKAN)
        glslMapFiles = shaderTools.generateShaderMaps(glslFiles, outDir, ShaderTools::FORMAT_GLSL);
#elif defined(NGFX_GRAPHICS_BACKEND_DIRECT3D12)
        hlslFiles = shaderTools.convertShaders(spvFiles, outDir, ShaderTools::FORMAT_HLSL);
        dxcFiles = shaderTools.compileShaders(hlslFiles, outDir, ShaderTools::FORMAT_HLSL);
        hlslMapFiles = shaderTools.generateShaderMaps(hlslFiles, outDir, ShaderTools::FORMAT_HLSL);
#elif defined(NGFX_GRAPHICS_BACKEND_METAL)
        mtlFiles = shaderTools.convertShaders(spvFiles, outDir, ShaderTools::FORMAT_MSL);
        mtllibFiles = shaderTools.compileShaders(mtlFiles, outDir, ShaderTools::FORMAT_MSL);
        mtlMapFiles = shaderTools.generateShaderMaps(mtlFiles, outDir, ShaderTools::FORMAT_MSL);
#endif
        return FileUtil::splitExt(spvFiles[0])[0];
    }
    string tmpDir;
    std::vector<string> glslFiles, spvFiles, glslMapFiles;
#if defined(NGFX_GRAPHICS_BACKEND_DIRECT3D12)
    std::vector<string> hlslFiles, dxcFiles, hlslMapFiles;
#elif defined(NGFX_GRAPHICS_BACKEND_METAL)
    std::vector<string> mtlFiles, mtllibFiles, mtlMapFiles;
#endif
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

