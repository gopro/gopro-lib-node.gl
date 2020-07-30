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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nodegl.h>

#include "ipc.h"

static void u32_write(uint8_t *buf, uint32_t v)
{
    buf[0] = v >> 24;
    buf[1] = v >> 16 & 0xff;
    buf[2] = v >>  8 & 0xff;
    buf[3] = v       & 0xff;
}

static void pkt_update_header(struct ipc_pkt *pkt)
{
    memcpy(pkt->data, "nglp", 4); // 'p' stands for packet
    u32_write(pkt->data + 4, pkt->size - 8);
}

struct ipc_pkt *ipc_pkt_create(void)
{
    struct ipc_pkt *pkt = malloc(sizeof(*pkt));
    if (!pkt)
        return NULL;
    pkt->size = 8;
    pkt->data = malloc(pkt->size);
    if (!pkt->data) {
        free(pkt);
        return NULL;
    }
    pkt_update_header(pkt);
    return pkt;
}

static int pack(struct ipc_pkt *pkt, uint32_t tag, const void *data, int datalen)
{
    uint8_t *dst = realloc(pkt->data, pkt->size + 8 + datalen);
    if (!dst)
        return NGL_ERROR_MEMORY;
    pkt->data = dst;
    u32_write(dst + pkt->size,     tag);
    u32_write(dst + pkt->size + 4, datalen);
    if (data)
        memcpy(dst + pkt->size + 8, data, datalen);
    pkt->size += 8 + datalen;
    pkt_update_header(pkt);
    return 0;
}

int ipc_pkt_add_qtag_scene(struct ipc_pkt *pkt, const char *scene)
{
    return pack(pkt, IPC_SCENE, scene, strlen(scene) + 1);
}

int ipc_pkt_add_qtag_file(struct ipc_pkt *pkt, const char *filename)
{
    return pack(pkt, IPC_FILE, filename, strlen(filename) + 1);
}

int ipc_pkt_add_qtag_filepart(struct ipc_pkt *pkt, const uint8_t *chunk, int chunk_size)
{
    return pack(pkt, IPC_FILEPART, chunk, chunk_size);
}

int ipc_pkt_add_qtag_duration(struct ipc_pkt *pkt, double duration)
{
    return pack(pkt, IPC_DURATION, &duration, sizeof(duration));
}

int ipc_pkt_add_qtag_aspect(struct ipc_pkt *pkt, const int *aspect)
{
    int ret = pack(pkt, IPC_ASPECT_RATIO, NULL, 2 * sizeof(*aspect));
    if (ret < 0)
        return ret;
    uint8_t *dst = pkt->data + pkt->size - 8;
    u32_write(dst,     aspect[0]);
    u32_write(dst + 4, aspect[1]);
    return 0;
}

int ipc_pkt_add_qtag_framerate(struct ipc_pkt *pkt, const int *framerate)
{
    int ret = pack(pkt, IPC_FRAMERATE, NULL, 2 * sizeof(*framerate));
    if (ret < 0)
        return ret;
    uint8_t *dst = pkt->data + pkt->size - 8;
    u32_write(dst,     framerate[0]);
    u32_write(dst + 4, framerate[1]);
    return 0;
}

int ipc_pkt_add_qtag_clearcolor(struct ipc_pkt *pkt, const float *clearcolor)
{
    return pack(pkt, IPC_CLEARCOLOR, clearcolor, 4 * sizeof(*clearcolor));
}

int ipc_pkt_add_qtag_samples(struct ipc_pkt *pkt, int samples)
{
    int ret = pack(pkt, IPC_SAMPLES, NULL, 1);
    if (ret < 0)
        return ret;
    uint8_t *dst = pkt->data + pkt->size - 1;
    *dst = samples;
    return 0;
}

int ipc_pkt_add_qtag_info(struct ipc_pkt *pkt)
{
    return pack(pkt, IPC_INFO, NULL, 0);
}

int ipc_pkt_add_qtag_reconfigure(struct ipc_pkt *pkt)
{
    return pack(pkt, IPC_RECONFIGURE, NULL, 0);
}

int ipc_pkt_add_rtag_info(struct ipc_pkt *pkt, const char *info)
{
    return pack(pkt, IPC_INFO, info, strlen(info) + 1);
}

int ipc_pkt_add_rtag_filepart(struct ipc_pkt *pkt, int written)
{
    int ret = pack(pkt, IPC_FILEPART, NULL, 4);
    if (ret < 0)
        return ret;
    uint8_t *dst = pkt->data + pkt->size - 4;
    u32_write(dst, written);
    return 0;
}

int ipc_pkt_add_rtag_fileend(struct ipc_pkt *pkt, const char *dest_filename)
{
    return pack(pkt, IPC_FILEEND, dest_filename, strlen(dest_filename) + 1);
}

void ipc_pkt_freep(struct ipc_pkt **pktp)
{
    struct ipc_pkt *pkt = *pktp;
    if (!pkt)
        return;
    free(pkt->data);
    free(pkt);
    *pktp = NULL;
}

int ipc_send(int fd, const struct ipc_pkt *pkt)
{
    const ssize_t n = write(fd, pkt->data, pkt->size);
    if (n < 0) {
        perror("write");
        return NGL_ERROR_IO;
    }
    // XXX: should we loop instead?
    if (n != pkt->size) {
        fprintf(stderr, "unable write packet (%zd/%d sent)\n", n, pkt->size);
        return NGL_ERROR_IO;
    }
    return 0;
}

static int readbuf(int fd, uint8_t *buf, ssize_t size)
{
    ssize_t nr = 0;
    while (nr != size) {
        const ssize_t n = read(fd, buf + nr, size - nr);
        if (n == 0)
            return 0;
        if (n < 0) {
            perror("read");
            return NGL_ERROR_IO;
        }
        nr += n;
    }
    return 1;
}

void ipc_pkt_reset(struct ipc_pkt *pkt)
{
    pkt->size = 8;
    pkt_update_header(pkt);
}

int ipc_recv(int fd, struct ipc_pkt *pkt)
{
    int ret = readbuf(fd, pkt->data, 8);
    if (ret <= 0)
        return ret;

    pkt->size = 8;

    if (memcmp(pkt->data, "nglp", 4))
        return NGL_ERROR_INVALID_DATA;

    const int size = IPC_U32_READ(pkt->data + 4);
    if (size == 0) // valid but empty packet
        return 0;

    if (size < 0)
        return NGL_ERROR_INVALID_DATA;

    uint8_t *dst = realloc(pkt->data, pkt->size + 8 + size);
    if (!dst)
        return NGL_ERROR_MEMORY;
    pkt->data = dst;

    ret = readbuf(fd, pkt->data + 8, size);
    if (ret <= 0)
        return ret;

    pkt->size += size;
    return ret;
}
