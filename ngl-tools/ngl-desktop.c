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

/*
 * To workaround a SDL issue¹ on macOS, we need to define _DARWIN_C_SOURCE when
 * _POSIX_C_SOURCE is also set to ensure that memset_pattern4() is defined. This
 * is mandatory because SDL_stdinc.h (included by SDL.h) uses memset_pattern4()
 * unconditionally on Apple systems.
 * [1]: https://bugzilla.libsdl.org/show_bug.cgi?id=5107
 */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L // for struct addrinfo with glibc

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#define SHUT_RDWR SD_BOTH
#else
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <nodegl.h>

#include "common.h"
#include "ipc.h"
#include "opts.h"
#include "player.h"
#include "pthread_compat.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct ctx {
    /* options */
    const char *host;
    const char *port;
    int log_level;
    struct ngl_config cfg;
    int aspect[2];
    int player_ui;
    int framerate[2];

    int sock_fd;
    struct addrinfo *addr_info;
    struct addrinfo *addr;

    char root_dir[1024];
    char session_file[1024];
    char files_dir[1024];
    struct player p;
    int thread_started;
    pthread_mutex_t lock;
    pthread_t thread;
    int stop_order;
    int own_session_file;
    struct ipc_pkt *send_pkt;
    struct ipc_pkt *recv_pkt;
    FILE *upload_fp;
    char upload_path[1024];
};

#define OFFSET(x) offsetof(struct ctx, x)
static const struct opt options[] = {
    {"-x", "--host",          OPT_TYPE_STR,      .offset=OFFSET(host)},
    {"-p", "--port",          OPT_TYPE_STR,      .offset=OFFSET(port)},
    {"-l", "--loglevel",      OPT_TYPE_LOGLEVEL, .offset=OFFSET(log_level)},
    {"-b", "--backend",       OPT_TYPE_BACKEND,  .offset=OFFSET(cfg.backend)},
    {"-s", "--size",          OPT_TYPE_RATIONAL, .offset=OFFSET(cfg.width)},
    {"-a", "--aspect",        OPT_TYPE_RATIONAL, .offset=OFFSET(aspect)},
    {"-z", "--swap_interval", OPT_TYPE_INT,      .offset=OFFSET(cfg.swap_interval)},
    {"-c", "--clear_color",   OPT_TYPE_COLOR,    .offset=OFFSET(cfg.clear_color)},
    {"-m", "--samples",       OPT_TYPE_INT,      .offset=OFFSET(cfg.samples)},
    {"-u", "--disable-ui",    OPT_TYPE_TOGGLE,   .offset=OFFSET(player_ui)},
    {"-r", "--framerate",     OPT_TYPE_RATIONAL, .offset=OFFSET(framerate)},
};

static int create_session_file(struct ctx *s)
{
    int fd = open(s->session_file, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        if (errno == EEXIST)
            fprintf(stderr, "ngl-desktop is already running on %s:%s, "
                    "delete %s if this is not the case\n", s->host, s->port, s->session_file);
        else
            perror("open");
        return NGL_ERROR_IO;
    }
    close(fd);
    s->own_session_file = 1;
    return 0;
}

static int remove_session_file(struct ctx *s)
{
    if (!s->own_session_file)
        return 0;
    int ret = unlink(s->session_file);
    if (ret < 0 && errno != EEXIST) {
        perror("unlink");
        return NGL_ERROR_IO;
    }
    return 0;
}

static int send_player_signal(enum player_signal sig, const void *data, int data_size)
{
    void *p = NULL;
    if (data_size) {
        p = malloc(data_size);
        memcpy(p, data, data_size);
    }

    SDL_Event event = {
        .user = {
            .type  = SDL_USEREVENT,
            .code  = sig,
            .data1 = p,
        },
    };
    if (SDL_PushEvent(&event) != 1)
        free(p);
    return 0;
}

static int handle_tag_scene(const uint8_t *data, int size)
{
    if (size < 1 || data[size - 1] != 0) // check if string is nul-terminated
        return NGL_ERROR_INVALID_DATA;
    const char *scene = (const char *)data;
    return send_player_signal(PLAYER_SIGNAL_SCENE, scene, size);
}

static int handle_tag_file(struct ctx *s, const uint8_t *data, int size)
{
    if (size < 1 || data[size - 1] != 0) // check if string is nul-terminated
        return NGL_ERROR_INVALID_DATA;

    if (s->upload_fp) {
        fprintf(stderr, "a file is already uploading");
        return NGL_ERROR_INVALID_USAGE;
    }

    /*
     * Basic (and probably too strict) check to make sure the file is not going
     * to be uploaded outside the files directory.
     *
     * TODO: we should use chdir()+chroot(), but it requires a switch to a
     * process model instead of threads (because the session file should not be
     * mixed with the uploaded files).
     */
    const char *filename = (const char *)data;
    if (strstr(filename, "..") || strchr(filename, '/')) {
        fprintf(stderr, "Only a filename is allowed\n");
        return NGL_ERROR_INVALID_ARG;
    }

    int ret = snprintf(s->upload_path, sizeof(s->upload_path), "%s%s", s->files_dir, filename);
    if (ret < 0 || ret >= sizeof(s->upload_path))
        return NGL_ERROR_MEMORY;

    s->upload_fp = fopen(s->upload_path, "wb");
    if (!s->upload_fp) {
        perror(s->upload_path);
        return NGL_ERROR_IO;
    }

    return 0;
}

static void close_upload_file(struct ctx *s)
{
    if (s->upload_fp)
        fclose(s->upload_fp);
    s->upload_fp = NULL;
}

static int handle_tag_filepart(struct ctx *s, const uint8_t *data, int size)
{
    if (!s->upload_fp) {
        fprintf(stderr, "file is not opened\n");
        return NGL_ERROR_INVALID_USAGE;
    }

    if (!size) {
        close_upload_file(s);
        return ipc_pkt_add_rtag_fileend(s->send_pkt, s->upload_path);
    }

    const size_t n = fwrite(data, 1, size, s->upload_fp);
    if (ferror(s->upload_fp)) {
        perror("fwrite");
        close_upload_file(s);
        return NGL_ERROR_IO;
    }
    // XXX: should we loop instead?
    if (n != size) {
        fprintf(stderr, "unable to write file part: %zd/%d written\n", n, size);
        close_upload_file(s);
        return NGL_ERROR_IO;
    }

    return ipc_pkt_add_rtag_filepart(s->send_pkt, size);
}

static int handle_tag_duration(const uint8_t *data, int size)
{
    if (size != 8)
        return NGL_ERROR_INVALID_DATA;
    return send_player_signal(PLAYER_SIGNAL_DURATION, data, size);
}

static int handle_tag_clearcolor(const uint8_t *data, int size)
{
    if (size != 16)
        return NGL_ERROR_INVALID_DATA;
    return send_player_signal(PLAYER_SIGNAL_CLEARCOLOR, data, 16);
}

static int handle_tag_samples(const uint8_t *data, int size)
{
    if (size != 1)
        return NGL_ERROR_INVALID_DATA;
    int samples = data[0];
    return send_player_signal(PLAYER_SIGNAL_SAMPLES, &samples, sizeof(samples));
}

static int handle_tag_aspect_ratio(const uint8_t *data, int size)
{
    if (size != 8)
        return NGL_ERROR_INVALID_DATA;
    const int aspect[2] = {IPC_U32_READ(data), IPC_U32_READ(data + 4)};
    return send_player_signal(PLAYER_SIGNAL_ASPECT_RATIO, aspect, sizeof(aspect));
}

static int handle_tag_framerate(const uint8_t *data, int size)
{
    if (size != 8)
        return NGL_ERROR_INVALID_DATA;
    const int rate[2] = {IPC_U32_READ(data), IPC_U32_READ(data + 4)};
    return send_player_signal(PLAYER_SIGNAL_FRAMERATE, rate, sizeof(rate));
}

static int handle_tag_reconfigure(const uint8_t *data, int size)
{
    if (size != 0)
        return NGL_ERROR_INVALID_DATA;
    return 0;
}

static const char *get_backend_str(int backend)
{
    static const char * const backend_map[] = {
        [NGL_BACKEND_OPENGL]   = "opengl",
        [NGL_BACKEND_OPENGLES] = "opengles",
    };

    if (backend < 0 || backend >= ARRAY_NB(backend_map))
        return NULL;
    const char *backend_str = backend_map[backend];
    if (!backend_str)
        return NULL;

    return backend_str;
}

static int handle_tag_info(const uint8_t *data, int size, int fd, struct ctx *s)
{
    if (size != 0)
        return NGL_ERROR_INVALID_DATA;
    /* Note: we use the player ngl_config and not the local config because the
     * former contains the backend in use after the configure call */
    const struct ngl_config *cfg = &s->p.ngl_config;
    const char *backend_str = get_backend_str(cfg->backend);
    if (!backend_str)
        return NGL_ERROR_BUG;

#ifdef _WIN32
    const char *sysname = "Windows";
#else
    struct utsname name;
    int ret = uname(&name);
    if (ret < 0)
        return NGL_ERROR_GENERIC;
    const char *sysname = name.sysname;
#endif

    char info[256];
    snprintf(info, sizeof(info), "backend=%s\nsystem=%s\n", backend_str, sysname);
    return ipc_pkt_add_rtag_info(s->send_pkt, info);
}

static int handle_commands(struct ctx *s, int fd)
{
    for (;;) {
        ipc_pkt_reset(s->send_pkt);

        int ret = ipc_recv(fd, s->recv_pkt);
        if (ret <= 0)
            return ret;

        int need_reconfigure = 0;
        const uint8_t *data = s->recv_pkt->data + 8;
        int data_size = s->recv_pkt->size - 8;
        while (data_size) {
            if (data_size < 8)
                return NGL_ERROR_INVALID_DATA;

            const enum ipc_tag tag = IPC_U32_READ(data);
            const int size         = IPC_U32_READ(data + 4);

            data += 8;
            data_size -= 8;

            if (size < 0 || size > data_size)
                return NGL_ERROR_INVALID_DATA;

            need_reconfigure |= tag == IPC_CLEARCOLOR || tag == IPC_SAMPLES || tag == IPC_RECONFIGURE;

            int ret;
            switch (tag) {
            case IPC_SCENE:        ret = handle_tag_scene(data, size);        break;
            case IPC_FILE:         ret = handle_tag_file(s, data, size);      break;
            case IPC_FILEPART:     ret = handle_tag_filepart(s, data, size);  break;
            case IPC_DURATION:     ret = handle_tag_duration(data, size);     break;
            case IPC_ASPECT_RATIO: ret = handle_tag_aspect_ratio(data, size); break;
            case IPC_FRAMERATE:    ret = handle_tag_framerate(data, size);    break;
            case IPC_CLEARCOLOR:   ret = handle_tag_clearcolor(data, size);   break;
            case IPC_SAMPLES:      ret = handle_tag_samples(data, size);      break;
            case IPC_RECONFIGURE:  ret = handle_tag_reconfigure(data, size);  break;
            case IPC_INFO:         ret = handle_tag_info(data, size, fd, s);  break;
            default:
                fprintf(stderr, "unrecognized query tag %c%c%c%c\n", IPC_U32_FMT(tag));
                return NGL_ERROR_INVALID_DATA;
            }
            if (ret < 0) {
                fprintf(stderr, "failed to handle query tag %c%c%c%c of size %d\n", IPC_U32_FMT(tag), size);
                return ret;
            }
            data += size;
            data_size -= size;
        }

        if (need_reconfigure) {
            ret = send_player_signal(PLAYER_SIGNAL_RECONFIGURE, NULL, 0);
            if (ret < 0)
                return ret;
        }

        ret = ipc_send(fd, s->send_pkt);
        if (ret < 0)
            return ret;
    }
}

static void close_socket(int socket)
{
    /*
     * On Solaris and MacOS, close() is the only way to terminate the
     * blocking accept(). The shutdown() call will fail with errno ENOTCONN
     * on these systems.
     *
     * On Linux though, shutdown() is required to terminate the blocking
     * accept(), because close() will not. The reason for this difference
     * of behaviour is to prevent a possible race scenario where the same
     * FD would be re-used between a close() and an accept().
     *
     * On Windows, the specific closesocket() function must be used instead of
     * close() to close a socket.
     *
     * See https://bugzilla.kernel.org/show_bug.cgi?id=106241 for more
     * information.
     */
    if (shutdown(socket, SHUT_RDWR) < 0)
        perror("shutdown");
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

static void close_conn(struct ctx *s, int conn_fd)
{
    close_socket(conn_fd);
    fprintf(stderr, "<< client %d disconnected\n", conn_fd);

    /* close uploading file when the connection ends */
    close_upload_file(s);
}

static void *server_start(void *arg)
{
    struct ctx *s = arg;

    for (;;) {
        const int conn_fd = accept(s->sock_fd, s->addr->ai_addr, &s->addr->ai_addrlen);
        if (conn_fd < 0) {
            perror("accept");
            break;
        }
        fprintf(stderr, ">> accepted client %d\n", conn_fd);

        pthread_mutex_lock(&s->lock);
        int stop = s->stop_order;
        pthread_mutex_unlock(&s->lock);
        if (stop) {
            close_conn(s, conn_fd);
            break;
        }

        int ret = handle_commands(s, conn_fd);
        if (ret < 0)
            fprintf(stderr, "client %d: error %d\n", conn_fd, ret);
        close_conn(s, conn_fd);
    }

    return NULL;
}

static void stop_server(struct ctx *s)
{
    pthread_mutex_lock(&s->lock);
    s->stop_order = 1;
    pthread_mutex_unlock(&s->lock);
}

static struct ngl_node *get_default_scene(const char *host, const char *port)
{
    char subtext_buf[64];
    snprintf(subtext_buf, sizeof(subtext_buf), "Listening on %s:%s", host, port);
    static const float fg_color[]  = {1.0, 2/3., 0.0, 1.0};
    static const float subtext_h[] = {0.0, 0.5, 0.0};

    struct ngl_node *group = ngl_node_create(NGL_NODE_GROUP);
    struct ngl_node *texts[] = {
        ngl_node_create(NGL_NODE_TEXT),
        ngl_node_create(NGL_NODE_TEXT),
    };

    if (!group || !texts[0] || !texts[1]) {
        ngl_node_unrefp(&group);
        goto end;
    }
    ngl_node_param_set(texts[0], "text", "No scene");
    ngl_node_param_set(texts[0], "fg_color", fg_color);
    ngl_node_param_set(texts[1], "text", subtext_buf);
    ngl_node_param_set(texts[1], "box_height", subtext_h);
    ngl_node_param_add(group, "children", 2, texts);

end:
    ngl_node_unrefp(&texts[0]);
    ngl_node_unrefp(&texts[1]);
    return group;
}

static int setup_network(struct ctx *s)
{
#ifdef _WIN32
    WSADATA wsa_data;
    int sret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (sret != 0) {
        fprintf(stderr, "WSAStartup: failed with %d\n", sret);
        return NGL_ERROR_IO;
    }
#endif

    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_flags    = AI_PASSIVE,
    };

    int ret = getaddrinfo(s->host, s->port, &hints, &s->addr_info);
    if (ret) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return NGL_ERROR_IO;
    }

    struct addrinfo *rp;
    for (rp = s->addr_info; rp; rp = rp->ai_next) {
        s->sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (s->sock_fd < 0)
            continue;

        int yes = 1;
        ret = setsockopt(s->sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(yes));
        if (ret == -1) {
            perror("setsockopt");
            break;
        }

        ret = bind(s->sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (ret != -1)
            break;

        close(s->sock_fd);
        s->sock_fd = -1;
    }

    if (!rp) {
        fprintf(stderr, "unable to bind\n");
        return NGL_ERROR_IO;
    }

    s->addr = rp;

    if (listen(s->sock_fd, 0) < 0) {
        perror("listen");
        return NGL_ERROR_IO;
    }

    return 0;
}

static int makedirs(const char *path, int mode)
{
    char cur_path[1024];

    int n = snprintf(cur_path, sizeof(cur_path), "%s", path);
    if (n != strlen(path)) {
        fprintf(stderr, "%s: path too long (%d > %zd)\n", path, n, strlen(path));
        return NGL_ERROR_MEMORY;
    }

    const char *p = cur_path;
    while (*p) {
        char *next = strchr(p, '/');
        if (!next) // we do not create the last element if not ending with a '/'
            break;
        if (p == next) { // ignore potential first '/'
            p = next + 1;
            continue;
        }
        *next = 0;
#ifdef _WIN32
        const int r = _mkdir(cur_path);
#else
        const int r = mkdir(cur_path, mode);
#endif
        *next = '/';
        if (r < 0 && errno != EEXIST) {
            perror(cur_path);
            return NGL_ERROR_GENERIC;
        }
        p = next + 1;
    }

    return 0;
}

static const char *env_vars[] = {"TMPDIR", "TEMP", "TMP"};

static const char *get_tmp_dir_from_env(void)
{
    for (int i = 0; i < ARRAY_NB(env_vars); i++) {
        const char *v = getenv(env_vars[i]);
        if (v)
            return v;
    }
    return NULL;
}

static int setup_paths(struct ctx *s)
{
#ifdef _WIN32
    TCHAR tmp_dir_default[MAX_PATH + 1];
    if (GetTempPath(sizeof(tmp_dir_default), tmp_dir_default) == 0)
        return NGL_ERROR_EXTERNAL;
#else
    static const char *tmp_dir_default = "/tmp";
#endif
    const char *tmp_dir = get_tmp_dir_from_env();
    if (!tmp_dir)
        tmp_dir = tmp_dir_default;

    int ret = snprintf(s->root_dir, sizeof(s->root_dir), "%s/ngl-desktop/%s-%s/", tmp_dir, s->host, s->port);
    if (ret < 0 || ret >= sizeof(s->root_dir))
        return ret;
    ret = snprintf(s->files_dir, sizeof(s->files_dir), "%sfiles/", s->root_dir);
    if (ret < 0 || ret >= sizeof(s->session_file))
        return ret;
    ret = makedirs(s->files_dir, 0700);
    if (ret < 0)
        return ret;
    ret = snprintf(s->session_file, sizeof(s->session_file), "%ssession", s->root_dir);
    if (ret < 0 || ret >= sizeof(s->session_file))
        return ret;
    return 0;
}

static int update_window_title(const struct ctx *s)
{
    const struct ngl_config *cfg = &s->p.ngl_config;
    const char *backend_str = get_backend_str(cfg->backend);
    if (!backend_str)
        return NGL_ERROR_BUG;

    char title[256];
    snprintf(title, sizeof(title), "ngl-desktop %s:%s [%s]", s->host, s->port, backend_str);
    SDL_SetWindowTitle(s->p.window, title);
    return 0;
}

int main(int argc, char *argv[])
{
    struct ctx s = {
        .host               = "localhost",
        .port               = "1234",
        .cfg.width          = DEFAULT_WIDTH,
        .cfg.height         = DEFAULT_HEIGHT,
        .log_level          = NGL_LOG_INFO,
        .aspect[0]          = 1,
        .aspect[1]          = 1,
        .cfg.swap_interval  = -1,
        .cfg.clear_color[3] = 1.f,
        .lock               = PTHREAD_MUTEX_INITIALIZER,
        .sock_fd            = -1,
        .player_ui          = 1,
        .framerate[0]       = 60,
        .framerate[1]       = 1,
    };

    int ret = opts_parse(argc, argc, argv, options, ARRAY_NB(options), &s);
    if (ret < 0 || ret == OPT_HELP) {
        opts_print_usage(argv[0], options, ARRAY_NB(options), NULL);
        return ret == OPT_HELP ? 0 : EXIT_FAILURE;
    }

    s.send_pkt = ipc_pkt_create();
    s.recv_pkt = ipc_pkt_create();
    if (!s.send_pkt || !s.recv_pkt) {
        ret = EXIT_FAILURE;
        goto end;
    }

    ngl_log_set_min_level(s.log_level);
    get_viewport(s.cfg.width, s.cfg.height, s.aspect, s.cfg.viewport);

    if ((ret = setup_paths(&s)) < 0 ||
        (ret = setup_network(&s)) < 0 ||
        (ret = create_session_file(&s)) < 0 ||
        (ret = pthread_create(&s.thread, NULL, server_start, &s)) < 0)
        goto end;
    s.thread_started = 1;

    struct ngl_node *scene = get_default_scene(s.host, s.port);
    ret = player_init(&s.p, "ngl-desktop", scene, &s.cfg, 0, s.framerate, s.player_ui);
    ngl_node_unrefp(&scene);
    if (ret < 0)
        goto end;

    ret = update_window_title(&s);
    if (ret < 0)
        goto end;

    player_main_loop();

end:
    remove_session_file(&s);

    if (s.thread_started)
        stop_server(&s);

    if (s.sock_fd != -1)
        close_socket(s.sock_fd);

    if (s.addr_info)
        freeaddrinfo(s.addr_info);

    if (s.thread_started)
        pthread_join(s.thread, NULL);

    pthread_mutex_destroy(&s.lock);

    ipc_pkt_freep(&s.send_pkt);
    ipc_pkt_freep(&s.recv_pkt);

    player_uninit();

    return ret;
}
