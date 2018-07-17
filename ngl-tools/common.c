/*
 * Copyright 2017 GoPro Inc.
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

#include <stdio.h>
#include <sys/time.h>

#ifdef VULKAN_BACKEND
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include "common.h"

int64_t gettime(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return 1000000 * (int64_t)tv.tv_sec + tv.tv_usec;
}

double clipd(double v, double min, double max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

int init_glfw(void)
{
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    return 0;
}

GLFWwindow *get_window(const char *title, int width, int height)
{
    GLFWwindow *window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        return NULL;
    }
    glfwShowWindow(window);
    return window;
}
