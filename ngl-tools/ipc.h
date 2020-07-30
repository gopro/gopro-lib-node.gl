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

#ifndef IPC_H
#define IPC_H

#include <stdint.h>

#define IPC_U32(a,b,c,d) (((uint32_t)(a))<<24 | (b)<<16 | (c)<<8 | (d))
#define IPC_U32_READ(buf) IPC_U32((buf)[0], (buf)[1], (buf)[2], (buf)[3])
#define IPC_U32_FMT(tag) (tag)>>24, (tag)>>16&0xff, (tag)>>8&0xff, (tag)&0xff

enum ipc_tag {
    IPC_SCENE        = IPC_U32('s','c','n','e'),
    IPC_FILE         = IPC_U32('f','i','l','e'),
    IPC_FILEPART     = IPC_U32('f','p','r','t'),
    IPC_FILEEND      = IPC_U32('f','e','n','d'),
    IPC_DURATION     = IPC_U32('d','u','r','t'),
    IPC_ASPECT_RATIO = IPC_U32('r','t','i','o'),
    IPC_FRAMERATE    = IPC_U32('r','a','t','e'),
    IPC_CLEARCOLOR   = IPC_U32('c','c','l','r'),
    IPC_SAMPLES      = IPC_U32('m','s','a','a'),
    IPC_INFO         = IPC_U32('i','n','f','o'),
    IPC_RECONFIGURE  = IPC_U32('r','c','f','g'),
};

struct ipc_pkt {
    uint8_t *data;
    int size;
};

struct ipc_pkt *ipc_pkt_create(void);
void ipc_pkt_reset(struct ipc_pkt *pkt);
void ipc_pkt_freep(struct ipc_pkt **pktp);

/* Query tags */
int ipc_pkt_add_qtag_scene(struct ipc_pkt *pkt, const char *scene);
int ipc_pkt_add_qtag_file(struct ipc_pkt *pkt, const char *filename);
int ipc_pkt_add_qtag_filepart(struct ipc_pkt *pkt, const uint8_t *chunk, int chunk_size);
int ipc_pkt_add_qtag_duration(struct ipc_pkt *pkt, double duration);
int ipc_pkt_add_qtag_aspect(struct ipc_pkt *pkt, const int *aspect);
int ipc_pkt_add_qtag_framerate(struct ipc_pkt *pkt, const int *framerate);
int ipc_pkt_add_qtag_clearcolor(struct ipc_pkt *pkt, const float *clearcolor);
int ipc_pkt_add_qtag_samples(struct ipc_pkt *pkt, int samples);
int ipc_pkt_add_qtag_info(struct ipc_pkt *pkt);
int ipc_pkt_add_qtag_reconfigure(struct ipc_pkt *pkt);

/* Response tags */
int ipc_pkt_add_rtag_info(struct ipc_pkt *pkt, const char *info);
int ipc_pkt_add_rtag_filepart(struct ipc_pkt *pkt, int written);
int ipc_pkt_add_rtag_fileend(struct ipc_pkt *pkt, const char *dest_filename);

int ipc_send(int fd, const struct ipc_pkt *pkt);
int ipc_recv(int fd, struct ipc_pkt *pkt);

#endif
