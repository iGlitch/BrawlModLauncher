#include "http.h"
#include <ctype.h>
#include <errno.h>
#include <strings.h>   // strncasecmp/strcasecmp
#include <network.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <wchar.h>
#include "networkloader.h"

// --- wolfSSL (TLS) ---
#include <wolfssl/options.h>
#include <wolfssl/ssl.h>
#include <wolfssl/error-ssl.h>

// ================= TLS configuration =================
#ifndef HTTP_TLS_VERIFY
#define HTTP_TLS_VERIFY 0   // 0 = don't verify server certs (default, "just works").
                            // Set to 1 and load a CA/pinned cert if you want verification.
#endif
// #define HTTP_TLS_DEBUG  // uncomment for verbose TLS logs
// =====================================================

bool _httpNetworkOK = false;
bool _httpNetInitComplete = false;
extern bool netBusy;
extern f32 g_LauncherVersion;

s32 _httpNetworkComplete(s32 ret, void *usrData)
{
    _httpNetworkOK = (ret == 0);
    _httpNetInitComplete = true;
    return 0;
}

// ------------------ wolfSSL glue (WOLFSSL_USER_IO) ------------------
static void ensure_wolfssl_init()
{
    static bool s_inited = false;
    if (!s_inited) {
        wolfSSL_Init();
#ifdef HTTP_TLS_DEBUG
        wolfSSL_Debugging_ON();
#endif
        s_inited = true;
    }
}

struct WolfIoCtx {
    int sock;
};

static int wolf_io_recv(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
    WolfIoCtx* io = (WolfIoCtx*)ctx;
    int r = net_read(io->sock, buf, sz);
    if (r > 0) return r;
    if (r == 0) return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    if (r == -EAGAIN || r == -EWOULDBLOCK || r == -EINTR) return WOLFSSL_CBIO_ERR_WANT_READ;
    return WOLFSSL_CBIO_ERR_GENERAL;
}

static int wolf_io_send(WOLFSSL* ssl, char* buf, int sz, void* ctx)
{
    WolfIoCtx* io = (WolfIoCtx*)ctx;
    int r = net_send(io->sock, buf, sz, 0);
    if (r >= 0) return r;
    if (r == -EAGAIN || r == -EWOULDBLOCK || r == -EINTR) return WOLFSSL_CBIO_ERR_WANT_WRITE;
    return WOLFSSL_CBIO_ERR_GENERAL;
}

static int http_send(int sock, WOLFSSL* ssl, const void* buf, int len)
{
    int sent = 0;
    while (sent < len) {
        int r = ssl ? wolfSSL_write(ssl, (const char*)buf + sent, len - sent)
                    : net_send(sock, (const char*)buf + sent, len - sent, 0);
        if (r > 0) { sent += r; continue; }
        if (ssl) {
            int err = wolfSSL_get_error(ssl, r);
            if (err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE) { usleep(1000); continue; }
        } else if (r == -EAGAIN || r == -EWOULDBLOCK || r == -EINTR) { usleep(1000); continue; }
        return r; // fatal
    }
    return sent;
}

static int http_recv(int sock, WOLFSSL* ssl, void* buf, int len)
{
    for (;;) {
        int r = ssl ? wolfSSL_read(ssl, buf, len)
                    : net_read(sock, buf, len);
        if (r > 0) return r;
        if (r == 0) return 0;
        if (ssl) {
            int err = wolfSSL_get_error(ssl, r);
            if (err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE) { usleep(1000); continue; }
        } else if (r == -EAGAIN || r == -EWOULDBLOCK || r == -EINTR) { usleep(1000); continue; }
        return r; // fatal
    }
}

// ------------------ URL / TCP helpers ------------------
static bool parse_url(const char* url, bool& useTLS, int& port, char* hostOut, size_t hostOutSz, const char*& pathOut)
{
    const char* p = NULL;
    if (strncmp(url, "https://", 8) == 0) { useTLS = true;  p = url + 8; port = 443; }
    else if (strncmp(url, "http://", 7) == 0) { useTLS = false; p = url + 7; port = 80; }
    else return false;

    const char* slash = strchr(p, '/');
    if (!slash) return false;

    size_t hostLen = (size_t)(slash - p);
    if (hostLen == 0 || hostLen >= hostOutSz) return false;
    memcpy(hostOut, p, hostLen);
    hostOut[hostLen] = '\0';

    char* colon = (char*)memchr(hostOut, ':', hostLen);
    if (colon) {
        *colon = '\0';
        int explicitPort = atoi(colon + 1);
        if (explicitPort > 0 && explicitPort < 65536) port = explicitPort;
    }

    pathOut = slash; // includes leading '/'
    return true;
}

static int tcp_connect(const char* host, int port)
{
    s32 sock = net_socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) return -1;

    struct hostent* hostEntry = net_gethostbyname((char*)host);
    if (!hostEntry) {
        net_close(sock);
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = PF_INET;
    memcpy((char*)&addr.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);
    addr.sin_port = htons((u16)port);

    s32 ret = net_connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr));
    if (ret < 0) {
        net_close(sock);
        return -1;
    }
    return sock;
}

// ------------------ TLS wrap/unwrap ------------------
static bool tls_wrap_socket(int sock, const char* host, WOLFSSL_CTX** outCtx, WOLFSSL** outSsl, WolfIoCtx** outIo)
{
    ensure_wolfssl_init();

    WOLFSSL_CTX* ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    if (!ctx) return false;

    wolfSSL_SetIORecv(ctx, wolf_io_recv);
    wolfSSL_SetIOSend(ctx, wolf_io_send);

#if HTTP_TLS_VERIFY
    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_PEER, 0);
    // TODO: load a CA bundle or pinned cert:
    // wolfSSL_CTX_load_verify_buffer(ctx, ca_der, ca_der_len, WOLFSSL_FILETYPE_ASN1);
#else
    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_NONE, 0);
#endif

    WOLFSSL* ssl = wolfSSL_new(ctx);
    if (!ssl) { wolfSSL_CTX_free(ctx); return false; }

    WolfIoCtx* io = (WolfIoCtx*)malloc(sizeof(WolfIoCtx));
    if (!io) { wolfSSL_free(ssl); wolfSSL_CTX_free(ctx); return false; }
    io->sock = sock;
    wolfSSL_SetIOReadCtx(ssl, io);
    wolfSSL_SetIOWriteCtx(ssl, io);

#if defined(HAVE_SNI) || defined(USE_SNI)
    wolfSSL_UseSNI(ssl, WOLFSSL_SNI_HOST_NAME, host, (unsigned)strlen(host));
#endif

    int ret = wolfSSL_connect(ssl);
    if (ret != WOLFSSL_SUCCESS) {
#ifdef HTTP_TLS_DEBUG
        int err = wolfSSL_get_error(ssl, ret);
        printf("wolfSSL_connect failed: %d\n", err);
#endif
        wolfSSL_free(ssl);
        wolfSSL_CTX_free(ctx);
        free(io);
        return false;
    }

    *outCtx = ctx; *outSsl = ssl; *outIo = io;
    return true;
}

static void tls_close(WOLFSSL_CTX* ctx, WOLFSSL* ssl, WolfIoCtx* io)
{
    if (ssl) wolfSSL_shutdown(ssl);
    if (ssl) wolfSSL_free(ssl);
    if (ctx) wolfSSL_CTX_free(ctx);
    if (io)  free(io);
}

// ------------------ small reader helpers ------------------
typedef struct {
    int sock; WOLFSSL* tls;
    char* buf; int cap;
    int pos; int avail;
} RStream;

static void rs_init(RStream* rs, int sock, WOLFSSL* tls, char* scratch, int cap)
{ rs->sock = sock; rs->tls = tls; rs->buf = scratch; rs->cap = cap; rs->pos = 0; rs->avail = 0; }

static int rs_fill(RStream* rs)
{ int r = http_recv(rs->sock, rs->tls, rs->buf, rs->cap); if (r <= 0) return r; rs->pos = 0; rs->avail = r; return r; }

static int rs_getbyte(RStream* rs, char* out)
{
    if (rs->pos < rs->avail) { *out = rs->buf[rs->pos++]; return 1; }
    int r = rs_fill(rs); if (r <= 0) return r;
    *out = rs->buf[rs->pos++]; return 1;
}

static int rs_readline(RStream* rs, char* out, int maxlen)
{
    int n = 0; char c = 0;
    while (n < maxlen - 1) {
        int r = rs_getbyte(rs, &c);
        if (r <= 0) return r;
        out[n++] = c;
        if (n >= 2 && out[n-2] == '\r' && out[n-1] == '\n') { out[n-2] = '\0'; return n - 2; }
    }
    out[n] = '\0'; return n;
}

static int rs_readn(RStream* rs, char* out, int need)
{
    int got = 0;
    while (got < need) {
        if (rs->pos < rs->avail) {
            int take = rs->avail - rs->pos;
            if (take > need - got) take = need - got;
            memcpy(out + got, rs->buf + rs->pos, take);
            rs->pos += take; got += take;
        } else {
            int r = rs_fill(rs); if (r <= 0) return (got > 0 ? got : r);
        }
    }
    return got;
}

static bool parse_status_code(const char* hdrStart, int hdrLen, int* outStatus)
{
    if (hdrLen < 12 || strncmp(hdrStart, "HTTP/", 5) != 0) return false;
    const char* sp = (const char*)memchr(hdrStart, ' ', hdrLen);
    if (!sp) return false;
    *outStatus = atoi(sp + 1);
    return (*outStatus > 0);
}

// ------------------ HTTP client functions ------------------

int downloadFileToBuffer(char * url, char ** buffer, wchar_t * sCurrentInfoText, bool &bForceCancel, f32 &fProgressPercentage)
{
    if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Connecting to WiFi...");
    fProgressPercentage = 0.0f;

    while (netBusy && !bForceCancel) usleep(1000);
    if (bForceCancel) return -1;
    netBusy = true;

    while (net_get_status() != 0 && !bForceCancel)
    {
        _httpNetInitComplete = false;
        while (net_get_status() == -EBUSY) usleep(50);
        net_init_async(_httpNetworkComplete, NULL);

        while (!_httpNetInitComplete && !bForceCancel) usleep(1000);

        if (!_httpNetworkOK)
        {
            while (net_get_status() == -EBUSY) usleep(50);
            net_deinit();
            net_wc24cleanup();
            sleep(2);
        }
        else break;
    }

    if (bForceCancel) {
        if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Cancelling...");
        netBusy = false;
        return -1;
    }

    // ---- Parse URL and connect (HTTP or HTTPS) ----
    bool useTLS = false; int port = 0; const char* path = NULL; char host[256];
    if (!parse_url(url, useTLS, port, host, sizeof(host), path)) { netBusy = false; return -1; }

    if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Connecting to server...");
    int sock = tcp_connect(host, port);
    if (sock < 0) { netBusy = false; return -1; }

    WOLFSSL_CTX* tlsCtx = NULL; WOLFSSL* tls = NULL; WolfIoCtx* io = NULL;
    if (useTLS) {
        if (!tls_wrap_socket(sock, host, &tlsCtx, &tls, &io)) {
            net_close(sock); netBusy = false; return -1;
        }
    }

    // ---- Send request ----
    const char* headerformat =
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: PMLauncher %4.2f\r\n"
        "Connection: close\r\n"
        "Accept: */*\r\n"
        "Accept-Encoding: identity\r\n"
        "\r\n";

    size_t hdrLenEst = strlen(headerformat) + strlen(host) + strlen(path) + 32;
    char* req = (char*)malloc(hdrLenEst);
    if (!req) { if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
    sprintf(req, headerformat, path, host, g_LauncherVersion);

    if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Requesting file...");
    if (http_send(sock, tls, req, (int)strlen(req)) < 0) {
        free(req); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1;
    }
    free(req);

    if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Downloading file information...");

    // ---- Read headers (accumulate until \r\n\r\n) ----
    int cap = 4096;
    char* hdrBuf = (char*)malloc(cap);
    if (!hdrBuf) { if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
    int have = 0;
    char* hdrEnd = NULL;

    while (!hdrEnd) {
        if (have == cap) { cap *= 2; char* grown = (char*)realloc(hdrBuf, cap); if (!grown) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; } hdrBuf = grown; }
        int r = http_recv(sock, tls, hdrBuf + have, cap - have);
        if (r <= 0) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
        have += r;
        hdrEnd = (char*)memmem(hdrBuf, have, "\r\n\r\n", 4);
        if (bForceCancel) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
    }

    int headerBytes = (int)(hdrEnd - hdrBuf);
    int status = 0;
    if (!parse_status_code(hdrBuf, headerBytes, &status) || status < 200 || status >= 300) {
        free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1;
    }

    // Parse headers
    bool hasLen = false, isChunked = false;
    u32 contentLen = 0;
    {
        char* p = hdrBuf;
        while (p < hdrBuf + headerBytes) {
            char* eol = (char*)memmem(p, (hdrBuf + headerBytes) - p, "\r\n", 2);
            if (!eol) break;
            int L = (int)(eol - p);
            if (L >= 15 && strncasecmp(p, "Content-Length:", 15) == 0) { contentLen = (u32)strtoul(p + 15, NULL, 10); hasLen = true; }
            else if (L >= 18 && strncasecmp(p, "Transfer-Encoding:", 18) == 0) { if (strcasestr(p, "chunked")) isChunked = true; }
            p = eol + 2;
        }
    }

    // Body bytes already in hdrBuf after \r\n\r\n
    const char* bodyStart = hdrEnd + 4;
    int initialBody = have - (int)(bodyStart - hdrBuf);

    // ---- Read body ----
    s32 bytesWritten = 0;
    void* outBuf = NULL;

    if (hasLen) {
        if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Downloading update... (0/%uKB)", contentLen/1024);
        fProgressPercentage = 0.0f;
        outBuf = malloc(contentLen);
        if (!outBuf) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }

        if (initialBody > 0) { memcpy(outBuf, bodyStart, initialBody); bytesWritten = initialBody; }

        char* scratch = (char*)malloc(4096);
        if (!scratch) { free(outBuf); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }

        while (bytesWritten < (s32)contentLen) {
            if (bForceCancel) { free(scratch); free(outBuf); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
            int toRead = ((s32)contentLen - bytesWritten);
            if (toRead > 4096) toRead = 4096;
            int r = http_recv(sock, tls, scratch, toRead);
            if (r <= 0) { free(scratch); free(outBuf); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
            memcpy((char*)outBuf + bytesWritten, scratch, r);
            bytesWritten += r;
            if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Downloading update... (%d/%d KB)", bytesWritten/1024, contentLen/1024);
            fProgressPercentage = (f32)bytesWritten / (f32)contentLen;
        }
        free(scratch);
    } else if (isChunked) {
        // chunked -> grow buffer as needed
        size_t capOut = (initialBody > 0 ? (size_t)initialBody : 0) + 4096;
        outBuf = malloc(capOut);
        if (!outBuf) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
        if (initialBody > 0) { memcpy(outBuf, bodyStart, initialBody); bytesWritten = initialBody; }

        // Seed RStream with any body already in hdrBuf
        RStream rs; rs_init(&rs, sock, tls, hdrBuf, have); // reuse hdrBuf as scratch
        // Move the leftover body into scratch buffer head
        if (initialBody > 0) {
            memmove(hdrBuf, bodyStart, initialBody);
            rs.pos = 0; rs.avail = initialBody;
        } else {
            rs.pos = have; rs.avail = have; // force fill on first read
            rs.pos = rs.avail = 0;
        }

        char line[64];
        for (;;) {
            int L = rs_readline(&rs, line, sizeof(line)); if (L <= 0) { free(outBuf); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
            long chunk = strtol(line, NULL, 16);
            if (chunk <= 0) {
                // read trailing headers until blank line
                do { L = rs_readline(&rs, line, sizeof(line)); if (L <= 0) break; } while (L != 0);
                break;
            }
            if ((size_t)(bytesWritten + chunk) > capOut) {
                while ((size_t)(bytesWritten + chunk) > capOut) capOut *= 2;
                void* grown = realloc(outBuf, capOut);
                if (!grown) { free(outBuf); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
                outBuf = grown;
            }
            if (rs_readn(&rs, (char*)outBuf + bytesWritten, (int)chunk) != chunk) {
                free(outBuf); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1;
            }
            bytesWritten += (int)chunk;
            // consume CRLF
            if (rs_readn(&rs, line, 2) != 2) { free(outBuf); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
        }
        fProgressPercentage = 1.0f;
    } else {
        // read-to-close
        size_t capOut = (initialBody > 0 ? (size_t)initialBody : 0) + 4096;
        outBuf = malloc(capOut);
        if (!outBuf) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
        if (initialBody > 0) { memcpy(outBuf, bodyStart, initialBody); bytesWritten = initialBody; }

        char* scratch = (char*)malloc(4096);
        if (!scratch) { free(outBuf); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }

        for (;;) {
            int r = http_recv(sock, tls, scratch, 4096);
            if (r <= 0) break;
            if ((size_t)(bytesWritten + r) > capOut) {
                while ((size_t)(bytesWritten + r) > capOut) capOut *= 2;
                void* grown = realloc(outBuf, capOut);
                if (!grown) { free(scratch); free(outBuf); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return -1; }
                outBuf = grown;
            }
            memcpy((char*)outBuf + bytesWritten, scratch, r);
            bytesWritten += r;
        }
        free(scratch);
        fProgressPercentage = 1.0f;
    }

    if (useTLS) tls_close(tlsCtx, tls, io);
    net_close(sock);
    free(hdrBuf);
    *buffer = (char*)outBuf;
    netBusy = false;
    return bytesWritten;
}

bool downloadFileToDisk(char * url, char * out, wchar_t * sCurrentInfoText, bool &bForceCancel, f32 &fProgressPercentage)
{
    if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Connecting to WiFi...");
    fProgressPercentage = 0.0f;

    while (netBusy && !bForceCancel) usleep(1000);
    if (bForceCancel) return false;
    netBusy = true;

    while (net_get_status() != 0 && !bForceCancel)
    {
        _httpNetInitComplete = false;
        while (net_get_status() == -EBUSY) usleep(50);
        net_init_async(_httpNetworkComplete, NULL);

        while (!_httpNetInitComplete && !bForceCancel) usleep(1000);

        if (!_httpNetworkOK)
        {
            while (net_get_status() == -EBUSY) usleep(50);
            net_deinit();
            net_wc24cleanup();
            sleep(2);
        }
        else break;
    }

    if (bForceCancel) {
        if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Cancelling...");
        netBusy = false;
        return false;
    }

    // ---- Parse URL / connect ----
    bool useTLS = false; int port = 0; const char* path = NULL; char host[256];
    if (!parse_url(url, useTLS, port, host, sizeof(host), path)) { netBusy = false; return false; }

    if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Connecting to server...");
    int sock = tcp_connect(host, port);
    if (sock < 0) { netBusy = false; return false; }

    WOLFSSL_CTX* tlsCtx = NULL; WOLFSSL* tls = NULL; WolfIoCtx* io = NULL;
    if (useTLS) {
        if (!tls_wrap_socket(sock, host, &tlsCtx, &tls, &io)) {
            net_close(sock); netBusy = false; return false;
        }
    }

    // ---- Send request ----
    const char* headerformat =
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: PMLauncher %4.2f\r\n"
        "Connection: close\r\n"
        "Accept: */*\r\n"
        "Accept-Encoding: identity\r\n"
        "\r\n";

    size_t hdrLenEst = strlen(headerformat) + strlen(host) + strlen(path) + 32;
    char* req = (char*)malloc(hdrLenEst);
    if (!req) { if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false; }
    sprintf(req, headerformat, path, host, g_LauncherVersion);

    if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Requesting file...");
    if (http_send(sock, tls, req, (int)strlen(req)) < 0) {
        free(req); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false;
    }
    free(req);

    if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Downloading file information...");

    // ---- Read headers ----
    int cap = 4096;
    char* hdrBuf = (char*)malloc(cap);
    if (!hdrBuf) { if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false; }
    int have = 0;
    char* hdrEnd = NULL;

    while (!hdrEnd) {
        if (have == cap) { cap *= 2; char* grown = (char*)realloc(hdrBuf, cap); if (!grown) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false; } hdrBuf = grown; }
        int r = http_recv(sock, tls, hdrBuf + have, cap - have);
        if (r <= 0) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false; }
        have += r;
        hdrEnd = (char*)memmem(hdrBuf, have, "\r\n\r\n", 4);
        if (bForceCancel) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false; }
    }

    int headerBytes = (int)(hdrEnd - hdrBuf);
    int status = 0;
    if (!parse_status_code(hdrBuf, headerBytes, &status) || status < 200 || status >= 300) {
        free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false;
    }

    bool hasLen = false, isChunked = false;
    u32 contentLen = 0;
    {
        char* p = hdrBuf;
        while (p < hdrBuf + headerBytes) {
            char* eol = (char*)memmem(p, (hdrBuf + headerBytes) - p, "\r\n", 2);
            if (!eol) break;
            int L = (int)(eol - p);
            if (L >= 15 && strncasecmp(p, "Content-Length:", 15) == 0) { contentLen = (u32)strtoul(p + 15, NULL, 10); hasLen = true; }
            else if (L >= 18 && strncasecmp(p, "Transfer-Encoding:", 18) == 0) { if (strcasestr(p, "chunked")) isChunked = true; }
            p = eol + 2;
        }
    }

    const char* bodyStart = hdrEnd + 4;
    int initialBody = have - (int)(bodyStart - hdrBuf);

    FILE* fDownload = fopen(out, "wb");
    if (!fDownload) { free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false; }

    // Write any body bytes already present
    if (initialBody > 0) {
        if ((int)fwrite(bodyStart, 1, initialBody, fDownload) != initialBody) {
            fclose(fDownload); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false;
        }
    }

    s32 bytesWritten = initialBody;
    bool failed = false;

    if (hasLen) {
        if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Downloading update... (%d/%d KB)", bytesWritten/1024, contentLen/1024);
        fProgressPercentage = (contentLen ? (f32)bytesWritten / (f32)contentLen : 0.0f);
        char* buf = (char*)malloc(4096);
        if (!buf) { fclose(fDownload); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false; }
        while (bytesWritten < (s32)contentLen) {
            if (failed || bForceCancel) break;
            int toRead = ((s32)contentLen - bytesWritten);
            if (toRead > 4096) toRead = 4096;
            int r = http_recv(sock, tls, buf, toRead);
            if (r <= 0) { failed = true; break; }
            if ((int)fwrite(buf, 1, r, fDownload) != r) { failed = true; break; }
            bytesWritten += r;
            if (sCurrentInfoText) swprintf(sCurrentInfoText, 255, L"Downloading update... (%d/%d KB)", bytesWritten/1024, contentLen/1024);
            fProgressPercentage = (f32)bytesWritten / (f32)contentLen;
            if (fsync(fileno(fDownload)) != 0) { failed = true; break; }
        }
        free(buf);
    } else if (isChunked) {
        // use hdrBuf as scratch for chunked reader
        RStream rs; rs_init(&rs, sock, tls, hdrBuf, have);
        // carry leftover initial body into scratch
        if (initialBody > 0) { memmove(hdrBuf, bodyStart, initialBody); rs.pos = 0; rs.avail = initialBody; } else { rs.pos = rs.avail = 0; }
        char line[64];
        for (;;) {
            int L = rs_readline(&rs, line, sizeof(line)); if (L <= 0) { failed = true; break; }
            long chunk = strtol(line, NULL, 16);
            if (chunk <= 0) {
                do { L = rs_readline(&rs, line, sizeof(line)); if (L <= 0) break; } while (L != 0);
                break;
            }
            int remain = (int)chunk;
            while (remain > 0) {
                int take = remain > 4096 ? 4096 : remain;
                char tmp[4096];
                int got = rs_readn(&rs, tmp, take);
                if (got != take) { failed = true; break; }
                if ((int)fwrite(tmp, 1, got, fDownload) != got) { failed = true; break; }
                bytesWritten += got; remain -= got;
            }
            if (failed) break;
            // consume CRLF
            if (rs_readn(&rs, line, 2) != 2) { failed = true; break; }
            // unknown total size -> progress indeterminate
        }
        fProgressPercentage = failed ? fProgressPercentage : 1.0f;
    } else {
        // read-to-close
        char* buf = (char*)malloc(4096);
        if (!buf) { fclose(fDownload); free(hdrBuf); if (useTLS) tls_close(tlsCtx,tls,io); net_close(sock); netBusy=false; return false; }
        for (;;) {
            int r = http_recv(sock, tls, buf, 4096);
            if (r <= 0) break;
            if ((int)fwrite(buf, 1, r, fDownload) != r) { failed = true; break; }
            bytesWritten += r;
        }
        free(buf);
        fProgressPercentage = failed ? fProgressPercentage : 1.0f;
    }

    if (failed || bForceCancel) {
        fclose(fDownload);
        free(hdrBuf);
        if (useTLS) tls_close(tlsCtx, tls, io);
        net_close(sock);
        netBusy = false;
        return false;
    }

    fsync(fileno(fDownload));
    fclose(fDownload);
    free(hdrBuf);
    if (useTLS) tls_close(tlsCtx, tls, io);
    net_close(sock);
    netBusy = false;
    return true;
}
