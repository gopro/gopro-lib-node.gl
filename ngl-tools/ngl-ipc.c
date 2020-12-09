/*
 * Copyright 2020 GoPro Inc.
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

#define _POSIX_C_SOURCE 200112L // for struct addrinfo with glibc

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Handleapi.h>
#include <Fileapi.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <nodegl.h>

#include "common.h"
#include "ipc.h"
#include "opts.h"

#define UPLOAD_CHUNK_SIZE (1024 * 1024)

struct ctx {
    /* options */
    const char *host;
    const char *port;
    const char *scene;
    int show_info;
    const char *uploadfile;
    double duration;
    int aspect[2];
    int framerate[2];
    float clear_color[4];
    int samples;
    int reconfigure;

    struct ipc_pkt *send_pkt;
    struct ipc_pkt *recv_pkt;
    FILE *upload_fp;
    uint8_t *upload_buffer;
    int64_t upload_size;
    int64_t uploaded_size;
};

#define OFFSET(x) offsetof(struct ctx, x)
static const struct opt options[] = {
    {"-x", "--host",          OPT_TYPE_STR,      .offset=OFFSET(host)},
    {"-p", "--port",          OPT_TYPE_STR,      .offset=OFFSET(port)},
    {"-f", "--scene",         OPT_TYPE_STR,      .offset=OFFSET(scene)},
    {"-?", "--info",          OPT_TYPE_TOGGLE,   .offset=OFFSET(show_info)},
    {"-u", "--uploadfile",    OPT_TYPE_STR,      .offset=OFFSET(uploadfile)},
    {"-t", "--duration",      OPT_TYPE_TIME,     .offset=OFFSET(duration)},
    {"-a", "--aspect",        OPT_TYPE_RATIONAL, .offset=OFFSET(aspect)},
    {"-r", "--framerate",     OPT_TYPE_RATIONAL, .offset=OFFSET(framerate)},
    {"-c", "--clearcolor",    OPT_TYPE_COLOR,    .offset=OFFSET(clear_color)},
    {"-m", "--samples",       OPT_TYPE_INT,      .offset=OFFSET(samples)},
    {"-g", "--reconfigure",   OPT_TYPE_TOGGLE,   .offset=OFFSET(reconfigure)},
};

static int get_filesize(const char *filename, int64_t *size)
{
#ifdef _WIN32
    HANDLE file_handle = CreateFile(TEXT(filename), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE)
        return NGL_ERROR_IO;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle, &file_size)) {
        CloseHandle(file_handle);
        return NGL_ERROR_IO;
    }
    *size = file_size.QuadPart;
    CloseHandle(file_handle);
#else
    struct stat st;
    int ret = stat(filename, &st);
    if (ret == -1) {
        perror(filename);
        return NGL_ERROR_IO;
    }
    *size = st.st_size;
#endif
    return 0;
}

static int craft_packet(struct ctx *s, struct ipc_pkt *pkt)
{
    if (s->scene) {
        char *serial_scene = get_text_file_content(strcmp(s->scene, "-") ? s->scene : NULL);
        if (!serial_scene)
            return -1;
        int ret = ipc_pkt_add_qtag_scene(pkt, serial_scene);
        free(serial_scene);
        if (ret < 0)
            return ret;
    }

    if (s->uploadfile) {
        char name[512];
        const size_t name_len = strcspn(s->uploadfile, "=");
        if (s->uploadfile[name_len] != '=') {
            fprintf(stderr, "upload file does not match \"remotename=localname\" format\n");
            return NGL_ERROR_INVALID_ARG;
        }
        if (name_len >= sizeof(name)) {
            fprintf(stderr, "remote file name too long %zd >= %zd\n", name_len, sizeof(name));
            return NGL_ERROR_MEMORY;
        }
        int n = snprintf(name, sizeof(name), "%.*s", (int)name_len, s->uploadfile);
        if (n < 0 || n >= sizeof(name))
            return NGL_ERROR_MEMORY;
        const size_t name_size = name_len + 1;

        const char *filename = s->uploadfile + name_size;
        int ret = get_filesize(filename, &s->upload_size);
        if (ret < 0)
            return ret;

        s->upload_fp = fopen(filename, "rb");
        if (!s->upload_fp) {
            perror("fopen");
            return NGL_ERROR_IO;
        }

        s->upload_buffer = malloc(UPLOAD_CHUNK_SIZE);
        if (!s->upload_buffer)
            return NGL_ERROR_MEMORY;

        ret = ipc_pkt_add_qtag_file(pkt, name);
        if (ret < 0)
            return ret;
    }

    if (s->duration >= 0.) {
        int ret = ipc_pkt_add_qtag_duration(pkt, s->duration);
        if (ret < 0)
            return ret;
    }

    if (s->aspect[0] > 0) {
        int ret = ipc_pkt_add_qtag_aspect(pkt, s->aspect);
        if (ret < 0)
            return ret;
    }

    if (s->framerate[0] > 0) {
        int ret = ipc_pkt_add_qtag_framerate(pkt, s->framerate);
        if (ret < 0)
            return ret;
    }

    if (s->clear_color[0] >= 0.) {
        int ret = ipc_pkt_add_qtag_clearcolor(pkt, s->clear_color);
        if (ret < 0)
            return ret;
    }

    if (s->samples >= 0) {
        int ret = ipc_pkt_add_qtag_samples(pkt, s->samples);
        if (ret < 0)
            return ret;
    }

    if (s->show_info) {
        int ret = ipc_pkt_add_qtag_info(pkt);
        if (ret < 0)
            return ret;
    }

    if (s->reconfigure) {
        int ret = ipc_pkt_add_qtag_reconfigure(pkt);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static void close_upload_file(struct ctx *s)
{
    if (s->upload_fp)
        fclose(s->upload_fp);
    s->upload_fp = NULL;
    free(s->upload_buffer);
    s->upload_buffer = NULL;
}

static int handle_info(const uint8_t *data, int size)
{
    if (size < 1 || data[size - 1] != 0) // check if string is nul-terminated
        return NGL_ERROR_INVALID_DATA;
    printf("%s", data);
    fflush(stdout);
    return 0;
}

static int handle_filepart(struct ctx *s, const uint8_t *data, int size)
{
    if (size != 4)
        return NGL_ERROR_INVALID_DATA;
    const int written = IPC_U32_READ(data);
    s->uploaded_size += written;
    fprintf(stderr, "\ruploading %s... %d%%", s->uploadfile, (int)(s->uploaded_size * 100LL / s->upload_size));
    return 0;
}

static int handle_fileend(struct ctx *s, const uint8_t *data, int size)
{
    if (size < 1 || data[size - 1] != 0) // check if string is nul-terminated
        return NGL_ERROR_INVALID_DATA;
    fprintf(stderr, "\ruploading %s... done\n", s->uploadfile);
    close_upload_file(s);
    const char *filename = (const char *)data;
    printf("%s\n", filename);
    return 0;
}

static int handle_response(struct ctx *s, const struct ipc_pkt *pkt)
{
    const uint8_t *data = pkt->data + 8;
    int data_size = pkt->size - 8;
    while (data_size) {
        if (data_size < 8)
            return NGL_ERROR_INVALID_DATA;

        const enum ipc_tag tag = IPC_U32_READ(data);
        const int size         = IPC_U32_READ(data + 4);

        data += 8;
        data_size -= 8;

        if (size < 0 || size > data_size)
            return NGL_ERROR_INVALID_DATA;

        int ret;
        switch (tag) {
        case IPC_INFO:      ret = handle_info(data, size);        break;
        case IPC_FILEPART:  ret = handle_filepart(s, data, size); break;
        case IPC_FILEEND:   ret = handle_fileend(s, data, size);  break;
        default:
            fprintf(stderr, "unrecognized response tag %c%c%c%c\n", IPC_U32_FMT(tag));
            return NGL_ERROR_INVALID_DATA;
        }
        if (ret < 0) {
            fprintf(stderr, "failed to handle response tag %c%c%c%c of size %d\n", IPC_U32_FMT(tag), size);
            return ret;
        }
        data += size;
        data_size -= size;
    }

    if (s->upload_fp) {
        ipc_pkt_reset(s->send_pkt);

        const size_t n = fread(s->upload_buffer, 1, UPLOAD_CHUNK_SIZE, s->upload_fp);
        if (ferror(s->upload_fp))
            return NGL_ERROR_IO;
        int ret = ipc_pkt_add_qtag_filepart(s->send_pkt, s->upload_buffer, n);
        if (ret < 0)
            return ret;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct ctx s = {
        .host           = "localhost",
        .port           = "1234",
        .duration       = -1.,
        .aspect[0]      = -1,
        .framerate[0]   = -1,
        .clear_color[0] = -1.,
        .samples        = -1,
    };

    int ret = opts_parse(argc, argc, argv, options, ARRAY_NB(options), &s);
    if (ret < 0 || ret == OPT_HELP) {
        opts_print_usage(argv[0], options, ARRAY_NB(options), NULL);
        return ret == OPT_HELP ? 0 : EXIT_FAILURE;
    }

#if _WIN32
    WSADATA wsa_data;
    int sret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (sret != 0) {
        fprintf(stderr, "WSAStartup: failed with %d\n", sret);
        return NGL_ERROR_IO;
    }
#endif

    struct addrinfo *addr_info = NULL;
    int fd = -1;

    s.send_pkt = ipc_pkt_create();
    s.recv_pkt = ipc_pkt_create();
    if (!s.send_pkt || !s.recv_pkt) {
        ret = NGL_ERROR_MEMORY;
        goto end;
    }

    ret = craft_packet(&s, s.send_pkt);
    if (ret < 0)
        goto end;

    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };

    ret = getaddrinfo(s.host, s.port, &hints, &addr_info);
    if (ret) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        ret = EXIT_FAILURE;
        goto end;
    }

    struct addrinfo *rp;
    for (rp = addr_info; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;

        ret = connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (ret != -1)
            break;

        close(fd);
    }

    if (!rp) {
        fprintf(stderr, "unable to connect to %s\n", s.host);
        ret = EXIT_FAILURE;
        goto end;
    }

    do {
        ret = ipc_send(fd, s.send_pkt);
        if (ret < 0)
            goto end;

        ret = ipc_recv(fd, s.recv_pkt);
        if (ret < 0)
            goto end;

        ret = handle_response(&s, s.recv_pkt);
        if (ret < 0)
            goto end;

    } while (s.upload_fp);

end:
    if (addr_info)
        freeaddrinfo(addr_info);
    close_upload_file(&s);
    ipc_pkt_freep(&s.send_pkt);
    ipc_pkt_freep(&s.recv_pkt);
    if (fd != -1)
        close(fd);
    return ret;
}
