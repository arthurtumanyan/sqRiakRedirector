/*
    sqRiakRedirector is a RIAK based SQUID rewrite software.
    Copyright (C) 2014  Arthur Tumanyan <arthurtumanyan@google.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <curl/curl.h>
#include <libconfig.h>

struct {
    char url[256];
    char client_ip[16 + 1];
    char fqdn[256];
    char user[16];
    char method[8];
    char kvpairs[64];
} URL_ENTRY;


static char p_string[1024];
static char config_file[256];

static char redirect_url[256];
static char rhost[17];
static char bucket[64];
static int rport = 8098;

static void halt();
static void * xmalloc(size_t);
static bool is_blocked(char *);
static void trim(const char *);
static void fill_fqdn_ip(char *);
static void purify_url(char *);
static void purify_url2(char *);
static void handler_sigint();
static void handler_sighup();
static void handler_sigquit();
static void handler_sigterm();
static void handler_sigusr();
static void set_sig_handler();
static void readConfig(char *);
static int is_valid_ip(const char *);
static size_t write_func(void *, size_t, size_t, void *);

const char ident[] = "SquriakRedirector";

int main(int argc, char **argv) {

    char tmp_fqdn_ip[272];
    int opt, sz = 0;

    memset(p_string, '\0', 1024);
    openlog(ident, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_INFO);
    set_sig_handler();

    if (argc != 2) {
        syslog(LOG_INFO, "Empty parameters\n");
        syslog(LOG_INFO, "Usage: redirector <config file>\n");
        halt();
    } else {
        sz = strlen(argv[1]);
        if (sz > 255)sz = 255;

        memset(config_file, '\0', 256);
        strncpy(config_file, argv[1], sz);
        readConfig(config_file);
    }
    //
    while (fgets(p_string, 1024, stdin) != NULL) {
        //
        memset(URL_ENTRY.url, '\0', 256);
        memset(URL_ENTRY.client_ip, '\0', 17);
        memset(URL_ENTRY.fqdn, '\0', 256);
        memset(URL_ENTRY.user, '\0', 16);
        memset(URL_ENTRY.method, '\0', 8);
        memset(URL_ENTRY.kvpairs, '\0', 64);
        //
        int scanned = sscanf(p_string, "%s %s %s %s %s",
                URL_ENTRY.url,
                tmp_fqdn_ip,
                URL_ENTRY.user,
                URL_ENTRY.method,
                URL_ENTRY.kvpairs
                );
        trim(p_string);

        if (0 > scanned && (0 != strcmp("Success", strerror(errno)))) {
            syslog(LOG_INFO, "Error: %s\n", strerror(errno));
            puts("");
            fflush(stdout);
            continue;
        } else if (scanned == 5) {
            fill_fqdn_ip(tmp_fqdn_ip);
            if (0 == strncmp(URL_ENTRY.method, "CONNECT", 7)) {
                purify_url(URL_ENTRY.url);
            } else {
                purify_url2(URL_ENTRY.url);
            }
            if (is_blocked(URL_ENTRY.url)) {
                fprintf(stdout, "%s\n", redirect_url);
            } else {
                puts("");
            }/* non blocked */
            fflush(stdout);
            continue;
        } else {
            puts("");
            fflush(stdout);
        }
    }/* while fgets... */
    halt();
    closelog();
}

static void halt() {
    syslog(LOG_INFO, "Terminating.");
    exit(EXIT_SUCCESS);
}

void trim(const char *str) {
    char *p;

    if ((p = strchr(str, '\r')) != NULL) {
        *p = '\0';
    }
    if ((p = strchr(str, '\n')) != NULL) {
        *p = '\0';
    }
}

static void fill_fqdn_ip(char *data) {
    if (NULL == data || 0 == strcmp(data, "")) {
        return;
    }
    char *duplicate = strdup(data);
    memset(URL_ENTRY.client_ip, '\0', 16);
    memset(URL_ENTRY.fqdn, '\0', 256);
    char *p = strtok(duplicate, "/");
    if (p != NULL) {
        int sz = strlen(p);
        if (sz > 15)sz = 15;
        snprintf(URL_ENTRY.client_ip, sz + 1, "%s", p);
    }
    p = strtok(NULL, "/");
    if (p != NULL) {
        int sz = strlen(p);
        if (sz > 254)sz = 254;
        snprintf(URL_ENTRY.fqdn, sz + 1, "%s", p);
    }
    if (duplicate)free(duplicate);
}

static void purify_url(char *data) {
    if (NULL == data || 0 == strcmp(data, "")) {
        return;
    }
    char * duplicate = strdup(data);
    char *p = strtok(duplicate, ":");
    if (p != NULL) {
        int sz = strlen(p);
        if (sz > 254)sz = 254;
        memset(URL_ENTRY.url, '\0', 256);
        snprintf(URL_ENTRY.url, sz + 1, "%s", p);
    }
    if (duplicate)free(duplicate);
}

static void purify_url2(char *data) {
    if (NULL == data || 0 == strcmp(data, "")) {
        return;
    }
    char * duplicate = strdup(data);
    char *p = strtok(duplicate, "/");
    if (p != NULL) {
        p = strtok(NULL, "/");
        if (p != NULL) {
            int sz = strlen(p);
            if (sz > 254)sz = 254;
            memset(URL_ENTRY.url, '\0', 256);
            snprintf(URL_ENTRY.url, sz + 1, "%s", p);
        }
    }
    if (duplicate)free(duplicate);
}

static void handler_sigint() {
    syslog(LOG_INFO, "Caught SIGINT signal!");
    halt();
}

static void handler_sigquit() {
    syslog(LOG_INFO, "Caught SIGQUIT signal!");
    halt();
}

static void handler_sigterm() {
    syslog(LOG_INFO, "Caught TERM signal!");
    halt();
}

static void handler_sighup() {
    syslog(LOG_INFO, "Caught SIGHUP signal!");
    readConfig(config_file);
    syslog(LOG_INFO, "Reconfigured!");
}

static void handler_sigusr() {
    syslog(LOG_INFO, "Caught SIGUSR signal!");
    halt();
}

static void set_sig_handler() {
    signal(SIGINT, handler_sigint);
    signal(SIGHUP, handler_sighup);
    signal(SIGTERM, handler_sigterm);
    signal(SIGQUIT, handler_sigquit);
    signal(SIGUSR1 | SIGUSR2, handler_sigusr);
    syslog(LOG_INFO, "Signal handlers installed!");
}

static int is_valid_ip(const char *str) {

    int segs = 0; /* Segment count. */
    int chcnt = 0; /* Character count within segment. */
    int accum = 0; /* Accumulator for segment. */

    /* Catch NULL pointer. */

    if (str == NULL)
        return 0;

    /* Process every character in string. */

    while (*str != '\0') {
        /* Segment changeover. */

        if (*str == '.') {
            /* Must have some digits in segment. */

            if (chcnt == 0)
                return 0;

            /* Limit number of segments. */

            if (++segs == 4)
                return 0;

            /* Reset segment values and restart loop. */

            chcnt = accum = 0;
            str++;
            continue;
        }

        /* Check numeric. */

        if ((*str < '0') || (*str > '9'))
            return 0;

        /* Accumulate and check segment. */

        if ((accum = accum * 10 + *str - '0') > 255)
            return 0;

        /* Advance other segment specific stuff and continue loop. */

        chcnt++;
        str++;
    }

    /* Check enough segments and enough characters in last segment. */

    if (segs != 3)
        return 0;

    if (chcnt == 0)
        return 0;

    /* Address okay. */

    return 1;
}

static bool is_blocked(char *url) {
    bool rv;
    CURLcode ret;
    CURL *hnd;
    long http_code = 0;
    char request[1024];
    int sz = strlen(rhost) + strlen(bucket) + strlen(url) + 30;
    snprintf(request, sz, "http://%s:%d/buckets/%s/keys/%s", rhost, rport, bucket, url);
    hnd = curl_easy_init();
    curl_easy_setopt(hnd, CURLOPT_URL, request);
    curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(hnd, CURLOPT_USERAGENT, ident);
    curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(hnd, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 2L);
    curl_easy_setopt(hnd, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(hnd, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_func);
    curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
    ret = curl_easy_perform(hnd);
    curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code == 200 && ret != CURLE_ABORTED_BY_CALLBACK) {
        rv = true;
    } else {
        rv = false;
    }
    curl_easy_cleanup(hnd);
    hnd = NULL;
    return rv;
}

static size_t write_func(void *ptr, size_t size, size_t nmemb, void *stream) {
    return 0;
}

static void readConfig(char * cfile) {
    config_t cfg, *cf;
    const config_setting_t * rules = NULL;
    const char * t_redirect_url = NULL;
    const char * t_bucket = NULL;
    const char * t_listen_host = NULL;

    int i = 0;
    cf = &cfg;

    config_init(cf);

    if (CONFIG_FALSE == config_read_file(cf, cfile)) {
        syslog(LOG_INFO, "Something wrong in %s, line #%d - %s. Exiting...\n", cfile, config_error_line(cf) - 1,
                config_error_text(cf));
        config_destroy(cf);
        exit(EXIT_FAILURE);
    } else {
        syslog(LOG_INFO, "Reading configuration file '%s'", cfile);
    }
    //
    t_redirect_url = xmalloc(sizeof (char) * 256);
    if (!config_lookup_string(cf, "redirect_url", &t_redirect_url)) {
        syslog(LOG_INFO, "'redirect_url' parameter not found\n");
        halt();
    } else {
        int redirect_sz = strlen(t_redirect_url);
        if (redirect_sz > 254)redirect_sz = 254;
        memset(redirect_url, '\0', 256);
        strncpy(redirect_url, t_redirect_url, (redirect_sz + 1));
        t_redirect_url = NULL;
    }
    //
    t_bucket = xmalloc(sizeof (char) * 64);
    if (!config_lookup_string(cf, "riak_bucket", &t_bucket)) {
        syslog(LOG_INFO, "'riak_bucket' parameter not found\n");
        halt();
    } else {
        int bucket_sz = strlen(t_bucket);
        if (bucket_sz > 62)bucket_sz = 62;
        memset(bucket, '\0', 64);
        strncpy(bucket, t_bucket, (bucket_sz + 1));
        t_bucket = NULL;
    }
    //
    t_listen_host = xmalloc(sizeof (char) * 16);
    if (!config_lookup_string(cf, "riak_host", &t_listen_host)) {
        syslog(LOG_INFO, "'riak_host' parameter not found\n");
        halt();
    } else {
        if (0 == is_valid_ip(t_listen_host)) {
            syslog(LOG_INFO, "Invalid ip address: %s\n", t_listen_host);
            halt();
        }
        int host_sz = strlen(t_listen_host);
        if (host_sz > 15)host_sz = 15;
        memset(rhost, '\0', 17);
        strncpy(rhost, t_listen_host, (host_sz + 1));
        t_listen_host = NULL;
    }
    //
    if (!config_lookup_int(cf, "riak_port", &rport)) {
        syslog(LOG_INFO, "Using default value of 'riak_port'\n");
    }
    /*
     * end of readConfig
     */
    config_destroy(cf);
}

static void * xmalloc(size_t size) {
    void * new_mem = (void *) malloc(size);
    if (new_mem == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        exit(EXIT_FAILURE);
    }
    return new_mem;
}