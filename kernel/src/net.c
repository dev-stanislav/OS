#include "net.h"
#include "io.h"
#include "libk.h"
#include "rtc.h"
#include "timer.h"
#include "bearssl.h"

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC
#define RTL_VENDOR_DEVICE 0x813910ECu
#define RTL_RX_BUF 8192
#define RTL_TX_BUF 1792

#define RTL_IDR0 0x00
#define RTL_TSD0 0x10
#define RTL_TSAD0 0x20
#define RTL_RBSTART 0x30
#define RTL_CMD 0x37
#define RTL_CAPR 0x38
#define RTL_IMR 0x3C
#define RTL_RCR 0x44

#define ETH_ARP 0x0806
#define ETH_IPV4 0x0800
#define IP_ICMP 1
#define IP_TCP 6
#define IP_UDP 17

#define DNS_SERVER0 10
#define DNS_SERVER1 0
#define DNS_SERVER2 2
#define DNS_SERVER3 3

#define TCP_MSS 1460
#define TCP_RX_BUF 8192
#define FETCH_MAX_BODY 4096
#define FETCH_MAX_RESPONSE 4608
#define FETCH_TIMEOUT_TICKS (12 * TIMER_HZ)
#define FETCH_RETRY_TICKS (2 * TIMER_HZ)

typedef enum { DNS_IDLE, DNS_WAIT, DNS_DONE, DNS_ERROR } dns_state_t;
typedef enum { TCP_CLOSED, TCP_SYN_SENT, TCP_ESTABLISHED, TCP_FIN_WAIT, TCP_DONE, TCP_ERROR } tcp_state_t;
typedef enum { FETCH_IDLE, FETCH_DNS, FETCH_CONNECT, FETCH_SEND, FETCH_RECV, FETCH_DONE, FETCH_ERROR } fetch_state_t;

typedef struct {
    dns_state_t state;
    uint16_t id;
    uint16_t sport;
    uint8_t ip[4];
    char host[96];
    uint8_t retries;
    uint32_t last_send;
    uint32_t deadline;
} dns_query_t;

typedef struct {
    tcp_state_t state;
    uint8_t remote_ip[4];
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t tx_seq;
    uint32_t tx_next;
    uint32_t rx_next;
    uint16_t remote_window;
    uint8_t tx_payload[TCP_MSS];
    uint16_t tx_length;
    uint32_t tx_deadline;
    uint8_t rx_data[TCP_RX_BUF];
    uint16_t rx_length;
    uint8_t connected_once;
} tcp_conn_t;

typedef struct {
    fetch_state_t state;
    char url[192];
    char host[96];
    char path[192];
    char error[80];
    uint8_t ip[4];
    uint16_t port;
    uint8_t https;
    uint8_t redirects;
    uint8_t request_sent;
    uint8_t printed_connect;
    char request[384];
    char response[FETCH_MAX_RESPONSE + 1];
    uint16_t response_length;
    char body[FETCH_MAX_BODY + 1];
    uint16_t body_length;
} fetch_job_t;

static uint16_t rtl_base;
static uint8_t rtl_found;
static uint8_t rx_buffer[RTL_RX_BUF + 16] __attribute__((aligned(16)));
static uint8_t tx_buffer[4][RTL_TX_BUF] __attribute__((aligned(4)));
static uint16_t rx_offset;
static uint8_t tx_index;
static const uint8_t local_ip[4] = {10, 0, 2, 15};
static const uint8_t gateway_ip[4] = {10, 0, 2, 2};
static const uint8_t dns_ip[4] = {DNS_SERVER0, DNS_SERVER1, DNS_SERVER2, DNS_SERVER3};
static uint8_t local_mac[6];
static uint8_t gateway_mac[6];
static uint8_t gateway_known;
static uint8_t ping_reply;
static uint16_t ping_sequence;
static uint8_t ping_target[4];
static uint8_t ping_pending;
static uint16_t ipv4_ident = 1;
static uint16_t udp_next_port = 49152;
static uint16_t tcp_next_port = 41000;
static dns_query_t dns_query;
static tcp_conn_t tcp_conn;
static fetch_job_t fetch_job;
static br_ssl_client_context tls_client;
static br_x509_minimal_context tls_x509;
static uint8_t tls_iobuf[BR_SSL_BUFSIZE_BIDI];
static uint16_t tls_request_offset;
static uint8_t tls_started;
static uint8_t tls_request_flushed;

static const unsigned char TA0_DN[] = {
    0x30, 0x47, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
    0x02, 0x55, 0x53, 0x31, 0x22, 0x30, 0x20, 0x06, 0x03, 0x55, 0x04, 0x0A,
    0x13, 0x19, 0x47, 0x6F, 0x6F, 0x67, 0x6C, 0x65, 0x20, 0x54, 0x72, 0x75,
    0x73, 0x74, 0x20, 0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x73, 0x20,
    0x4C, 0x4C, 0x43, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03,
    0x13, 0x0B, 0x47, 0x54, 0x53, 0x20, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x52,
    0x34
};

static const unsigned char TA0_EC_Q[] = {
    0x04, 0xF3, 0x74, 0x73, 0xA7, 0x68, 0x8B, 0x60, 0xAE, 0x43, 0xB8, 0x35,
    0xC5, 0x81, 0x30, 0x7B, 0x4B, 0x49, 0x9D, 0xFB, 0xC1, 0x61, 0xCE, 0xE6,
    0xDE, 0x46, 0xBD, 0x6B, 0xD5, 0x61, 0x18, 0x35, 0xAE, 0x40, 0xDD, 0x73,
    0xF7, 0x89, 0x91, 0x30, 0x5A, 0xEB, 0x3C, 0xEE, 0x85, 0x7C, 0xA2, 0x40,
    0x76, 0x3B, 0xA9, 0xC6, 0xB8, 0x47, 0xD8, 0x2A, 0xE7, 0x92, 0x91, 0x6A,
    0x73, 0xE9, 0xB1, 0x72, 0x39, 0x9F, 0x29, 0x9F, 0xA2, 0x98, 0xD3, 0x5F,
    0x5E, 0x58, 0x86, 0x65, 0x0F, 0xA1, 0x84, 0x65, 0x06, 0xD1, 0xDC, 0x8B,
    0xC9, 0xC7, 0x73, 0xC8, 0x8C, 0x6A, 0x2F, 0xE5, 0xC4, 0xAB, 0xD1, 0x1D,
    0x8A
};

static const unsigned char TA1_DN[] = {
    0x30, 0x2E, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
    0x02, 0x55, 0x53, 0x31, 0x0D, 0x30, 0x0B, 0x06, 0x03, 0x55, 0x04, 0x0A,
    0x13, 0x04, 0x49, 0x53, 0x52, 0x47, 0x31, 0x10, 0x30, 0x0E, 0x06, 0x03,
    0x55, 0x04, 0x03, 0x13, 0x07, 0x52, 0x6F, 0x6F, 0x74, 0x20, 0x59, 0x52
};

static const unsigned char TA1_RSA_N[] = {
    0xDB, 0xC6, 0x26, 0x73, 0x7B, 0xF0, 0x24, 0xC9, 0x75, 0x62, 0xF7, 0xF9,
    0xE1, 0x9F, 0xB0, 0xB3, 0x79, 0x4E, 0xB3, 0x41, 0x26, 0xCF, 0x95, 0x1F,
    0xD8, 0x51, 0x5E, 0xA4, 0x5B, 0xC3, 0x1B, 0xBD, 0xB0, 0x63, 0x62, 0x07,
    0x40, 0x43, 0xD5, 0xF7, 0x0E, 0xC5, 0xB4, 0x94, 0x40, 0x22, 0x48, 0x33,
    0x5C, 0x44, 0xD7, 0x70, 0xDB, 0xFB, 0x90, 0xB0, 0xD7, 0x0D, 0x2C, 0xD0,
    0x40, 0x58, 0xB2, 0xFB, 0x88, 0x3F, 0xFE, 0xA3, 0xA0, 0x5D, 0x30, 0xF1,
    0xCB, 0x88, 0x99, 0xB8, 0x11, 0xD4, 0xE1, 0xA0, 0x61, 0xB4, 0x90, 0xE0,
    0xEA, 0x73, 0x23, 0xE0, 0xC6, 0xF1, 0x21, 0xAE, 0x4E, 0x57, 0x04, 0xF3,
    0xBD, 0xCE, 0x09, 0x2F, 0xA4, 0x87, 0x7B, 0x2B, 0x96, 0x8E, 0xAF, 0x97,
    0x8D, 0xCC, 0xE4, 0xE2, 0x60, 0xE0, 0x07, 0xA8, 0xD6, 0xC7, 0xC7, 0xA7,
    0xA9, 0x13, 0x24, 0x3A, 0x08, 0x88, 0x50, 0x4D, 0x24, 0x06, 0x3E, 0x38,
    0xA7, 0xD7, 0xFC, 0x55, 0x2F, 0x60, 0xAB, 0xA1, 0x8D, 0x3F, 0xA7, 0xA3,
    0x8F, 0x65, 0x9A, 0xA9, 0xAE, 0xA5, 0x20, 0x48, 0xE4, 0xF9, 0x01, 0x42,
    0x2A, 0xAB, 0x10, 0x6B, 0x56, 0x53, 0x9B, 0xB1, 0x53, 0xF7, 0x10, 0x58,
    0x71, 0xAE, 0xF2, 0x34, 0xA3, 0x14, 0x1C, 0xE7, 0x66, 0xAB, 0xDB, 0x34,
    0xF2, 0xCC, 0x5C, 0xC2, 0xC5, 0xC4, 0x26, 0xF7, 0x59, 0x37, 0xEB, 0x24,
    0x15, 0xD7, 0x8E, 0xAB, 0xD6, 0x1B, 0xEB, 0xF8, 0x6E, 0x3C, 0xF1, 0x8E,
    0x37, 0x80, 0xB4, 0xE9, 0x54, 0xEE, 0xA6, 0xAB, 0x44, 0x3B, 0xCD, 0x3B,
    0x20, 0x2E, 0x41, 0x82, 0xE5, 0x9F, 0xDD, 0x38, 0x33, 0xE7, 0xDA, 0x2B,
    0x1F, 0x21, 0x9C, 0xB0, 0xA9, 0x27, 0x5E, 0x59, 0xA9, 0xA1, 0x20, 0x70,
    0xD9, 0x58, 0xFB, 0xD3, 0x0C, 0x59, 0xC0, 0xB9, 0xCA, 0xF2, 0xF6, 0x03,
    0x68, 0xF7, 0x97, 0xDF, 0xB6, 0x6E, 0xE8, 0x20, 0x65, 0x7F, 0x9D, 0x38,
    0x4F, 0x75, 0xCD, 0x89, 0x88, 0x75, 0xAC, 0x13, 0xBD, 0x26, 0x6E, 0x56,
    0xF9, 0x5B, 0x46, 0xB0, 0x25, 0x2F, 0x12, 0x01, 0xEA, 0x9C, 0x0C, 0x06,
    0x39, 0xDE, 0x5C, 0x28, 0xD8, 0x43, 0xCF, 0xB7, 0x04, 0x20, 0xA3, 0xB2,
    0xC6, 0xD2, 0x36, 0x96, 0x54, 0xDE, 0x14, 0xDC, 0x04, 0x37, 0x61, 0x7C,
    0x09, 0x44, 0x0E, 0x56, 0x2B, 0xEA, 0x91, 0x1A, 0xB6, 0x73, 0x1D, 0x98,
    0x34, 0xF9, 0x06, 0xA9, 0xCA, 0x0B, 0xDD, 0x9B, 0x77, 0x41, 0x21, 0x33,
    0x57, 0xDB, 0x2C, 0x78, 0xA8, 0x2D, 0xC7, 0x0B, 0x0A, 0xE8, 0x9E, 0xBD,
    0x3F, 0x19, 0x90, 0xF8, 0x59, 0xAE, 0xAE, 0xCC, 0x8A, 0x2E, 0xE0, 0x35,
    0x70, 0x1C, 0x50, 0x40, 0x11, 0xF3, 0xE9, 0x73, 0x61, 0xA9, 0x7E, 0xF7,
    0x34, 0xE0, 0xEC, 0x2E, 0x43, 0x00, 0xBC, 0x95, 0x39, 0xE1, 0xBF, 0x0A,
    0xA7, 0xD1, 0x00, 0xC9, 0xF5, 0x29, 0x21, 0xC2, 0x47, 0x43, 0x52, 0x03,
    0xA3, 0xFC, 0xB6, 0x7F, 0xE6, 0x19, 0x19, 0x67, 0x09, 0xCB, 0x79, 0xAE,
    0xD5, 0x42, 0x2E, 0x75, 0x72, 0x49, 0x5A, 0x54, 0xE8, 0x7D, 0x6A, 0x1A,
    0x0D, 0xC3, 0x81, 0x33, 0xAE, 0xB2, 0x47, 0xFC, 0x79, 0xA8, 0x53, 0x32,
    0x26, 0x1B, 0x23, 0x97, 0xB3, 0x3C, 0x99, 0x8E, 0xE5, 0xEA, 0xED, 0x6A,
    0x5B, 0x24, 0x43, 0x5B, 0xAD, 0xA7, 0xBE, 0x3E, 0x78, 0x3C, 0xBD, 0xD8,
    0xF2, 0x38, 0x7E, 0xD1, 0xCA, 0x4D, 0xBA, 0xBD, 0x21, 0x68, 0x47, 0xE1,
    0x86, 0x4A, 0x87, 0x75, 0xA7, 0x24, 0x42, 0x2D, 0xFA, 0x84, 0x2F, 0x95,
    0x86, 0x5B, 0x63, 0xCB, 0x69, 0x12, 0xBA, 0x0A, 0xAC, 0x50, 0x7A, 0x3C,
    0x51, 0xB3, 0xAC, 0x92, 0x03, 0x47, 0x2B, 0x6C, 0xE0, 0x7A, 0xAF, 0xB8,
    0x7E, 0x76, 0x44, 0x58, 0xF6, 0xCE, 0xFF, 0xC1
};

static const unsigned char TA1_RSA_E[] = {
    0x01, 0x00, 0x01
};

static const br_x509_trust_anchor TAs[2] = {
    {
        { (unsigned char *)TA0_DN, sizeof TA0_DN },
        BR_X509_TA_CA,
        {
            BR_KEYTYPE_EC,
            { .ec = {
                BR_EC_secp384r1,
                (unsigned char *)TA0_EC_Q, sizeof TA0_EC_Q,
            } }
        }
    },
    {
        { (unsigned char *)TA1_DN, sizeof TA1_DN },
        BR_X509_TA_CA,
        {
            BR_KEYTYPE_RSA,
            { .rsa = {
                (unsigned char *)TA1_RSA_N, sizeof TA1_RSA_N,
                (unsigned char *)TA1_RSA_E, sizeof TA1_RSA_E,
            } }
        }
    }
};

static uint16_t rd16(const uint8_t *p) { return ((uint16_t)p[0] << 8) | p[1]; }
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)rd16(p) << 16) | rd16(p + 2); }
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, (uint16_t)(v >> 16)); wr16(p + 2, (uint16_t)v); }
static void copy_mac(uint8_t *out, const uint8_t *in) { for (uint8_t i = 0; i < 6; i++) out[i] = in[i]; }
static uint8_t ip_equal(const uint8_t *a, const uint8_t *b) { for (uint8_t i = 0; i < 4; i++) if (a[i] != b[i]) return 0; return 1; }
static uint8_t is_digit(char c) { return c >= '0' && c <= '9'; }
static char lower_char(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static uint8_t text_starts(const char *text, const char *prefix) {
    while (*prefix) if (*text++ != *prefix++) return 0;
    return 1;
}
static uint8_t text_starts_ci(const char *text, const char *prefix) {
    while (*prefix) if (lower_char(*text++) != lower_char(*prefix++)) return 0;
    return 1;
}
static char *find_text(char *text, const char *needle) {
    size_t nl = kstrlen(needle);
    if (!nl) return text;
    for (size_t i = 0; text[i]; i++) {
        size_t j = 0;
        while (j < nl && text[i + j] == needle[j]) j++;
        if (j == nl) return text + i;
    }
    return 0;
}
static char *find_header(char *headers, const char *name) {
    size_t nl = kstrlen(name);
    char *line = headers;
    while (*line) {
        if (text_starts_ci(line, name) && line[nl] == ':') return line + nl + 1;
        char *next = find_text(line, "\r\n");
        if (!next) break;
        line = next + 2;
    }
    return 0;
}
static uint16_t decimal_parse(const char *text) {
    uint32_t value = 0;
    while (*text == ' ') text++;
    while (is_digit(*text)) {
        value = value * 10u + (uint32_t)(*text++ - '0');
        if (value > 65535u) return 65535;
    }
    return (uint16_t)value;
}
static void text_append(char *out, uint16_t cap, const char *text) {
    size_t len = kstrlen(out);
    kstrcpy(out + len, text, cap > len ? cap - len : 0);
}

static uint16_t checksum(const uint8_t *data, uint16_t length) {
    uint32_t sum = 0;
    while (length > 1) { sum += rd16(data); data += 2; length -= 2; }
    if (length) sum += (uint16_t)data[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)~sum;
}

static uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    outl(PCI_ADDR, 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)device << 11) | ((uint32_t)function << 8) | (offset & 0xFC));
    return inl(PCI_DATA);
}
static void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    outl(PCI_ADDR, 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)device << 11) | ((uint32_t)function << 8) | (offset & 0xFC));
    outl(PCI_DATA, value);
}

static void rtl_send(const uint8_t *frame, uint16_t length) {
    if (!rtl_found || length > RTL_TX_BUF) return;
    uint8_t slot = tx_index++ & 3;
    kmemcpy(tx_buffer[slot], frame, length);
    outl(rtl_base + RTL_TSAD0 + slot * 4, (uint32_t)tx_buffer[slot]);
    outl(rtl_base + RTL_TSD0 + slot * 4, length);
}

static void send_arp_request(const uint8_t *target_ip) {
    uint8_t frame[42];
    for (uint8_t i = 0; i < 6; i++) frame[i] = 0xFF;
    copy_mac(frame + 6, local_mac);
    wr16(frame + 12, ETH_ARP);
    wr16(frame + 14, 1);
    wr16(frame + 16, ETH_IPV4);
    frame[18] = 6; frame[19] = 4;
    wr16(frame + 20, 1);
    copy_mac(frame + 22, local_mac);
    kmemcpy(frame + 28, local_ip, 4);
    kmemset(frame + 32, 0, 6);
    kmemcpy(frame + 38, target_ip, 4);
    rtl_send(frame, sizeof(frame));
}

static uint8_t ensure_gateway(void) {
    if (gateway_known) return 1;
    send_arp_request(gateway_ip);
    return 0;
}

static void send_ipv4(uint8_t protocol, const uint8_t *dst_ip, const uint8_t *payload, uint16_t payload_len) {
    if (!ensure_gateway() || payload_len + 34 > RTL_TX_BUF) return;
    uint8_t frame[RTL_TX_BUF];
    copy_mac(frame, gateway_mac);
    copy_mac(frame + 6, local_mac);
    wr16(frame + 12, ETH_IPV4);
    uint8_t *ip = frame + 14;
    ip[0] = 0x45;
    ip[1] = 0;
    wr16(ip + 2, (uint16_t)(20 + payload_len));
    wr16(ip + 4, ipv4_ident++);
    wr16(ip + 6, 0x4000);
    ip[8] = 64;
    ip[9] = protocol;
    wr16(ip + 10, 0);
    kmemcpy(ip + 12, local_ip, 4);
    kmemcpy(ip + 16, dst_ip, 4);
    wr16(ip + 10, checksum(ip, 20));
    kmemcpy(ip + 20, payload, payload_len);
    rtl_send(frame, (uint16_t)(14 + 20 + payload_len));
}

static uint16_t udp_checksum(const uint8_t *dst_ip, const uint8_t *udp, uint16_t udp_len) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 4; i += 2) sum += rd16(local_ip + i);
    for (uint8_t i = 0; i < 4; i += 2) sum += rd16(dst_ip + i);
    sum += IP_UDP;
    sum += udp_len;
    const uint8_t *p = udp;
    uint16_t len = udp_len;
    while (len > 1) { sum += rd16(p); p += 2; len -= 2; }
    if (len) sum += (uint16_t)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)~sum;
}

static void send_udp(const uint8_t *dst_ip, uint16_t sport, uint16_t dport, const uint8_t *payload, uint16_t payload_len) {
    uint8_t packet[1500];
    uint16_t udp_len = (uint16_t)(8 + payload_len);
    wr16(packet, sport);
    wr16(packet + 2, dport);
    wr16(packet + 4, udp_len);
    wr16(packet + 6, 0);
    kmemcpy(packet + 8, payload, payload_len);
    wr16(packet + 6, udp_checksum(dst_ip, packet, udp_len));
    send_ipv4(IP_UDP, dst_ip, packet, udp_len);
}

static uint16_t tcp_checksum(const uint8_t *dst_ip, const uint8_t *tcp, uint16_t tcp_len) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 4; i += 2) sum += rd16(local_ip + i);
    for (uint8_t i = 0; i < 4; i += 2) sum += rd16(dst_ip + i);
    sum += IP_TCP;
    sum += tcp_len;
    const uint8_t *p = tcp;
    uint16_t len = tcp_len;
    while (len > 1) { sum += rd16(p); p += 2; len -= 2; }
    if (len) sum += (uint16_t)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)~sum;
}

static void tcp_send_flags(tcp_conn_t *c, uint8_t flags, const uint8_t *payload, uint16_t payload_len) {
    uint8_t packet[1500];
    uint16_t header_len = (flags & 0x02) ? 24 : 20;
    uint16_t tcp_len = (uint16_t)(header_len + payload_len);
    wr16(packet, c->local_port);
    wr16(packet + 2, c->remote_port);
    wr32(packet + 4, c->tx_next);
    wr32(packet + 8, c->rx_next);
    packet[12] = (uint8_t)(header_len / 4u) << 4;
    packet[13] = flags;
    wr16(packet + 14, 4096);
    wr16(packet + 16, 0);
    wr16(packet + 18, 0);
    if (flags & 0x02) {
        packet[20] = 2;
        packet[21] = 4;
        wr16(packet + 22, TCP_MSS);
    }
    if (payload_len) kmemcpy(packet + header_len, payload, payload_len);
    wr16(packet + 16, tcp_checksum(c->remote_ip, packet, tcp_len));
    send_ipv4(IP_TCP, c->remote_ip, packet, tcp_len);
}

static void send_ping(void) {
    if (!gateway_known) return;
    uint8_t packet[28];
    uint8_t *ip_payload = packet;
    ip_payload[0] = 8;
    ip_payload[1] = 0;
    wr16(ip_payload + 2, 0);
    wr16(ip_payload + 4, 0x4D4F);
    wr16(ip_payload + 6, ping_sequence++);
    wr16(ip_payload + 2, checksum(ip_payload, 8));
    send_ipv4(IP_ICMP, ping_target, ip_payload, 8);
}

static uint8_t parse_ip(const char *text, uint8_t *ip) {
    for (uint8_t part = 0; part < 4; part++) {
        uint16_t value = 0;
        uint8_t digits = 0;
        while (is_digit(*text)) {
            value = (uint16_t)(value * 10 + (*text++ - '0'));
            digits++;
            if (value > 255) return 0;
        }
        if (!digits) return 0;
        ip[part] = (uint8_t)value;
        if (part < 3) { if (*text++ != '.') return 0; }
        else if (*text) return 0;
    }
    return 1;
}

static uint16_t dns_write_name(uint8_t *out, const char *host) {
    uint16_t written = 0;
    const char *label = host;
    while (*label) {
        const char *end = label;
        while (*end && *end != '.') end++;
        uint8_t len = (uint8_t)(end - label);
        out[written++] = len;
        for (uint8_t i = 0; i < len; i++) out[written++] = (uint8_t)label[i];
        label = *end == '.' ? end + 1 : end;
    }
    out[written++] = 0;
    return written;
}

static uint8_t dns_skip_name(const uint8_t *packet, uint16_t length, uint16_t *offset) {
    uint16_t pos = *offset;
    uint8_t hops = 0;
    while (pos < length) {
        uint8_t n = packet[pos++];
        if ((n & 0xC0) == 0xC0) {
            if (pos >= length) return 0;
            pos++;
            *offset = pos;
            return 1;
        }
        if ((n & 0xC0) != 0) return 0;
        if (n == 0) { *offset = pos; return 1; }
        pos = (uint16_t)(pos + n);
        if (++hops > 64 || pos > length) return 0;
    }
    return 0;
}

static uint8_t dns_read_name(const uint8_t *packet, uint16_t length, uint16_t *offset, char *out, uint16_t cap) {
    uint16_t pos = *offset;
    uint16_t out_pos = 0;
    uint8_t jumped = 0;
    uint8_t hops = 0;
    while (pos < length) {
        uint8_t n = packet[pos++];
        if ((n & 0xC0) == 0xC0) {
            if (pos >= length) return 0;
            uint16_t ptr = (uint16_t)(((n & 0x3F) << 8) | packet[pos++]);
            if (!jumped) *offset = pos;
            pos = ptr;
            jumped = 1;
            if (++hops > 16) return 0;
            continue;
        }
        if ((n & 0xC0) != 0) return 0;
        if (n == 0) {
            if (!jumped) *offset = pos;
            if (out_pos == 0) kstrcpy(out, ".", cap);
            else out[out_pos ? out_pos - 1 : 0] = '\0';
            return 1;
        }
        if (pos + n > length) return 0;
        for (uint8_t i = 0; i < n && out_pos + 2 < cap; i++) out[out_pos++] = (char)packet[pos + i];
        if (out_pos + 1 < cap) out[out_pos++] = '.';
        pos = (uint16_t)(pos + n);
    }
    return 0;
}

static void dns_send_query(void) {
    uint8_t packet[320];
    kmemset(packet, 0, sizeof(packet));
    wr16(packet, dns_query.id);
    wr16(packet + 2, 0x0100);
    wr16(packet + 4, 1);
    uint16_t offset = 12;
    offset = (uint16_t)(offset + dns_write_name(packet + offset, dns_query.host));
    wr16(packet + offset, 1);
    wr16(packet + offset + 2, 1);
    send_udp(dns_ip, dns_query.sport, 53, packet, (uint16_t)(offset + 4));
    dns_query.last_send = timer_ticks();
}

static void dns_start(const char *host) {
    kmemset(&dns_query, 0, sizeof(dns_query));
    dns_query.state = DNS_WAIT;
    dns_query.id = (uint16_t)(0x5100u + (timer_ticks() & 0xFFu));
    dns_query.sport = udp_next_port++;
    dns_query.deadline = timer_ticks() + FETCH_TIMEOUT_TICKS;
    kstrcpy(dns_query.host, host, sizeof(dns_query.host));
    dns_send_query();
}

static void dns_handle(const uint8_t *payload, uint16_t length, uint16_t sport, uint16_t dport) {
    if (dns_query.state != DNS_WAIT || dport != dns_query.sport || sport != 53 || length < 12) return;
    if (rd16(payload) != dns_query.id || (payload[3] & 0x0F) != 0) { dns_query.state = DNS_ERROR; return; }
    uint16_t qd = rd16(payload + 4);
    uint16_t an = rd16(payload + 6);
    uint16_t off = 12;
    for (uint16_t i = 0; i < qd; i++) {
        if (!dns_skip_name(payload, length, &off) || off + 4 > length) { dns_query.state = DNS_ERROR; return; }
        off = (uint16_t)(off + 4);
    }
    for (uint16_t i = 0; i < an; i++) {
        char name[96];
        if (!dns_read_name(payload, length, &off, name, sizeof(name)) || off + 10 > length) { dns_query.state = DNS_ERROR; return; }
        uint16_t type = rd16(payload + off);
        uint16_t cls = rd16(payload + off + 2);
        uint16_t rdlen = rd16(payload + off + 8);
        off = (uint16_t)(off + 10);
        if (off + rdlen > length) { dns_query.state = DNS_ERROR; return; }
        if (type == 1 && cls == 1 && rdlen == 4) {
            kmemcpy(dns_query.ip, payload + off, 4);
            dns_query.state = DNS_DONE;
            return;
        }
        off = (uint16_t)(off + rdlen);
    }
    dns_query.state = DNS_ERROR;
}

static void dns_poll(void) {
    if (dns_query.state != DNS_WAIT) return;
    uint32_t now = timer_ticks();
    if (now > dns_query.deadline) { dns_query.state = DNS_ERROR; return; }
    if (now - dns_query.last_send > FETCH_RETRY_TICKS) {
        if (++dns_query.retries > 4) { dns_query.state = DNS_ERROR; return; }
        dns_send_query();
    }
}

static void tcp_reset(void) { kmemset(&tcp_conn, 0, sizeof(tcp_conn)); tcp_conn.state = TCP_CLOSED; }

static void tcp_connect(const uint8_t *ip, uint16_t port) {
    tcp_reset();
    kmemcpy(tcp_conn.remote_ip, ip, 4);
    tcp_conn.remote_port = port;
    tcp_conn.local_port = tcp_next_port++;
    tcp_conn.tx_seq = 0x10000000u + (timer_ticks() * 97u) + tcp_conn.local_port;
    tcp_conn.tx_next = tcp_conn.tx_seq;
    tcp_conn.rx_next = 0;
    tcp_conn.state = TCP_SYN_SENT;
    tcp_conn.tx_deadline = timer_ticks() + FETCH_TIMEOUT_TICKS;
    tcp_send_flags(&tcp_conn, 0x02, 0, 0);
}

static uint8_t seq_acked(uint32_t ack, uint32_t seq_end) {
    return (int32_t)(ack - seq_end) >= 0;
}

static uint8_t tcp_send_data(const uint8_t *data, uint16_t length) {
    if (tcp_conn.state != TCP_ESTABLISHED || tcp_conn.tx_length || length > TCP_MSS) return 0;
    kmemcpy(tcp_conn.tx_payload, data, length);
    tcp_conn.tx_length = length;
    tcp_send_flags(&tcp_conn, 0x18, tcp_conn.tx_payload, tcp_conn.tx_length);
    tcp_conn.tx_deadline = timer_ticks() + FETCH_TIMEOUT_TICKS;
    return 1;
}

static uint16_t tcp_read(uint8_t *out, uint16_t capacity) {
    uint16_t n = tcp_conn.rx_length < capacity ? tcp_conn.rx_length : capacity;
    if (!n) return 0;
    kmemcpy(out, tcp_conn.rx_data, n);
    if (n < tcp_conn.rx_length) kmemcpy(tcp_conn.rx_data, tcp_conn.rx_data + n, (uint16_t)(tcp_conn.rx_length - n));
    tcp_conn.rx_length = (uint16_t)(tcp_conn.rx_length - n);
    return n;
}

static void tcp_poll(void) {
    if (tcp_conn.state == TCP_SYN_SENT && timer_ticks() > tcp_conn.tx_deadline) tcp_conn.state = TCP_ERROR;
    if (tcp_conn.state == TCP_ESTABLISHED && tcp_conn.tx_length && timer_ticks() > tcp_conn.tx_deadline) tcp_conn.state = TCP_ERROR;
}

static void tcp_handle(const uint8_t *ip, const uint8_t *tcp, uint16_t tcp_len) {
    if (tcp_len < 20 || tcp_conn.state == TCP_CLOSED) return;
    uint16_t sport = rd16(tcp);
    uint16_t dport = rd16(tcp + 2);
    if (dport != tcp_conn.local_port || sport != tcp_conn.remote_port || !ip_equal(ip + 12, tcp_conn.remote_ip)) return;
    uint8_t header_len = (uint8_t)((tcp[12] >> 4) * 4);
    if (header_len < 20 || header_len > tcp_len) return;
    uint8_t flags = tcp[13];
    uint32_t seq = rd32(tcp + 4);
    uint32_t ack = rd32(tcp + 8);
    uint16_t payload_len = (uint16_t)(tcp_len - header_len);
    const uint8_t *payload = tcp + header_len;
    tcp_conn.remote_window = rd16(tcp + 14);
    if (flags & 0x04) { tcp_conn.state = TCP_ERROR; return; }
    if (tcp_conn.state == TCP_SYN_SENT) {
        if ((flags & 0x12) == 0x12 && ack == tcp_conn.tx_seq + 1) {
            tcp_conn.rx_next = seq + 1;
            tcp_conn.tx_next = tcp_conn.tx_seq + 1;
            tcp_conn.state = TCP_ESTABLISHED;
            tcp_conn.connected_once = 1;
            tcp_send_flags(&tcp_conn, 0x10, 0, 0);
        }
        return;
    }
    if ((flags & 0x10) && tcp_conn.tx_length && seq_acked(ack, tcp_conn.tx_next + tcp_conn.tx_length)) {
        tcp_conn.tx_next += tcp_conn.tx_length;
        tcp_conn.tx_length = 0;
    }
    if (payload_len && seq == tcp_conn.rx_next) {
        uint16_t space = (uint16_t)(sizeof(tcp_conn.rx_data) - tcp_conn.rx_length);
        if (payload_len <= space) {
            kmemcpy(tcp_conn.rx_data + tcp_conn.rx_length, payload, payload_len);
            tcp_conn.rx_length = (uint16_t)(tcp_conn.rx_length + payload_len);
            tcp_conn.rx_next += payload_len;
        }
        tcp_send_flags(&tcp_conn, 0x10, 0, 0);
    } else if (payload_len) tcp_send_flags(&tcp_conn, 0x10, 0, 0);
    if (flags & 0x01) {
        tcp_conn.rx_next++;
        tcp_send_flags(&tcp_conn, 0x10, 0, 0);
        tcp_conn.state = TCP_DONE;
    }
}

static void fetch_error(const char *message) {
    fetch_job.state = FETCH_ERROR;
    kstrcpy(fetch_job.error, message, sizeof(fetch_job.error));
}

static void tls_reset_state(void) {
    tls_request_offset = 0;
    tls_started = 0;
    tls_request_flushed = 0;
}

static uint8_t parse_url(const char *url) {
    const char *p = url;
    kmemset(fetch_job.host, 0, sizeof(fetch_job.host));
    kmemset(fetch_job.path, 0, sizeof(fetch_job.path));
    fetch_job.https = 0;
    fetch_job.port = 80;
    if (text_starts(p, "http://")) p += 7;
    else if (text_starts(p, "https://")) { p += 8; fetch_job.https = 1; fetch_job.port = 443; }
    else return 0;
    uint16_t hi = 0;
    while (*p && *p != '/' && *p != ':' && hi + 1 < sizeof(fetch_job.host)) fetch_job.host[hi++] = *p++;
    fetch_job.host[hi] = '\0';
    if (!hi) return 0;
    if (*p == ':') {
        p++;
        uint16_t port = 0;
        while (is_digit(*p)) port = (uint16_t)(port * 10 + (*p++ - '0'));
        if (!port) return 0;
        fetch_job.port = port;
    }
    if (*p == '/') kstrcpy(fetch_job.path, p, sizeof(fetch_job.path));
    else kstrcpy(fetch_job.path, "/", sizeof(fetch_job.path));
    return 1;
}

static void fetch_build_request(void) {
    fetch_job.request[0] = '\0';
    text_append(fetch_job.request, sizeof(fetch_job.request), "GET ");
    text_append(fetch_job.request, sizeof(fetch_job.request), fetch_job.path);
    text_append(fetch_job.request, sizeof(fetch_job.request), " HTTP/1.1\r\nHost: ");
    text_append(fetch_job.request, sizeof(fetch_job.request), fetch_job.host);
    text_append(fetch_job.request, sizeof(fetch_job.request), "\r\nUser-Agent: MiniOS/1\r\nAccept: */*\r\nConnection: close\r\n\r\n");
}

static uint8_t fetch_parse_response(void);
static uint8_t fetch_start_url(const char *url, uint8_t redirects);

static uint8_t fetch_append_response(const uint8_t *data, uint16_t length) {
    if (fetch_job.response_length + length > FETCH_MAX_RESPONSE) {
        fetch_error("response too large");
        return 0;
    }
    kmemcpy(fetch_job.response + fetch_job.response_length, data, length);
    fetch_job.response_length = (uint16_t)(fetch_job.response_length + length);
    return 1;
}

static void tls_make_seed(uint8_t *seed, uint16_t length) {
    uint32_t values[8];
    values[0] = timer_ticks();
    values[1] = rtc_days_since_year0();
    values[2] = rtc_seconds_of_day();
    values[3] = ((uint32_t)local_mac[0] << 24) | ((uint32_t)local_mac[1] << 16) | ((uint32_t)local_mac[2] << 8) | local_mac[3];
    values[4] = ((uint32_t)local_mac[4] << 24) | ((uint32_t)local_mac[5] << 16) | ((uint32_t)tcp_conn.local_port << 1);
    values[5] = tcp_conn.tx_seq;
    values[6] = tcp_conn.rx_next;
    values[7] = (uint32_t)(uintptr_t)seed;
    for (uint16_t i = 0; i < length; i++) {
        uint32_t v = values[i & 7] + 0x9E3779B9u * (uint32_t)(i + 1);
        seed[i] = (uint8_t)(v >> ((i & 3) * 8));
        values[i & 7] = (v << 7) ^ (v >> 3) ^ timer_ticks();
    }
}

static uint8_t tls_start(void) {
    if (!rtc_ready()) { fetch_error("RTC time unavailable"); return 0; }
    uint8_t seed[48];
    tls_reset_state();
    kmemset(&tls_client, 0, sizeof(tls_client));
    kmemset(&tls_x509, 0, sizeof(tls_x509));
    br_ssl_client_init_full(&tls_client, &tls_x509, TAs, (sizeof TAs) / (sizeof TAs[0]));
    br_x509_minimal_set_time(&tls_x509, rtc_days_since_year0(), rtc_seconds_of_day());
    br_ssl_engine_set_buffer(&tls_client.eng, tls_iobuf, sizeof(tls_iobuf), 1);
    tls_make_seed(seed, sizeof(seed));
    br_ssl_engine_inject_entropy(&tls_client.eng, seed, sizeof(seed));
    if (!br_ssl_client_reset(&tls_client, fetch_job.host, 0)) { fetch_error("TLS init failed"); return 0; }
    tls_started = 1;
    return 1;
}

static void tls_pump(void) {
    if (!tls_started || fetch_job.state == FETCH_ERROR || fetch_job.state == FETCH_DONE) return;
    for (uint8_t iter = 0; iter < 64; iter++) {
        unsigned state = br_ssl_engine_current_state(&tls_client.eng);
        uint8_t progress = 0;
        if (state == BR_SSL_CLOSED) {
            if (br_ssl_engine_last_error(&tls_client.eng) != 0) fetch_error("TLS failed");
            else if (fetch_parse_response()) fetch_job.state = FETCH_DONE;
            return;
        }
        if ((state & BR_SSL_SENDREC) && tcp_conn.tx_length == 0 && tcp_conn.state == TCP_ESTABLISHED) {
            size_t len = 0;
            unsigned char *buf = br_ssl_engine_sendrec_buf(&tls_client.eng, &len);
            if (buf && len) {
                uint16_t n = len > TCP_MSS ? TCP_MSS : (uint16_t)len;
                if (tcp_send_data(buf, n)) {
                    br_ssl_engine_sendrec_ack(&tls_client.eng, n);
                    progress = 1;
                    continue;
                }
            }
        }
        if (state & BR_SSL_RECVREC) {
            size_t len = 0;
            unsigned char *buf = br_ssl_engine_recvrec_buf(&tls_client.eng, &len);
            if (buf && len) {
                uint16_t want = len > 512 ? 512 : (uint16_t)len;
                uint16_t got = tcp_read(buf, want);
                if (got) {
                    br_ssl_engine_recvrec_ack(&tls_client.eng, got);
                    progress = 1;
                    continue;
                }
            }
        }
        if ((state & BR_SSL_SENDAPP) && tls_request_offset < kstrlen(fetch_job.request)) {
            size_t len = 0;
            unsigned char *buf = br_ssl_engine_sendapp_buf(&tls_client.eng, &len);
            if (buf && len) {
                uint16_t remaining = (uint16_t)(kstrlen(fetch_job.request) - tls_request_offset);
                uint16_t n = len > remaining ? remaining : (uint16_t)len;
                kmemcpy(buf, fetch_job.request + tls_request_offset, n);
                br_ssl_engine_sendapp_ack(&tls_client.eng, n);
                tls_request_offset = (uint16_t)(tls_request_offset + n);
                progress = 1;
                continue;
            }
        }
        if (tls_request_offset == kstrlen(fetch_job.request) && !tls_request_flushed) {
            br_ssl_engine_flush(&tls_client.eng, 0);
            tls_request_flushed = 1;
            fetch_job.state = FETCH_RECV;
            progress = 1;
            continue;
        }
        if (state & BR_SSL_RECVAPP) {
            size_t len = 0;
            unsigned char *buf = br_ssl_engine_recvapp_buf(&tls_client.eng, &len);
            if (buf && len) {
                uint16_t n = len > 512 ? 512 : (uint16_t)len;
                if (!fetch_append_response(buf, n)) return;
                br_ssl_engine_recvapp_ack(&tls_client.eng, n);
                progress = 1;
                continue;
            }
        }
        if (!progress) break;
    }
    if (tcp_conn.state == TCP_ERROR) { fetch_error("TCP failed"); return; }
    if (tcp_conn.state == TCP_DONE) {
        if (fetch_job.response_length) {
            if (fetch_parse_response()) fetch_job.state = FETCH_DONE;
        } else fetch_error("TLS closed");
    }
}

static uint8_t hex_value(char c, uint8_t *out) {
    if (c >= '0' && c <= '9') { *out = (uint8_t)(c - '0'); return 1; }
    if (c >= 'a' && c <= 'f') { *out = (uint8_t)(c - 'a' + 10); return 1; }
    if (c >= 'A' && c <= 'F') { *out = (uint8_t)(c - 'A' + 10); return 1; }
    return 0;
}

static uint16_t hex_parse_line(const char *text) {
    uint32_t value = 0;
    uint8_t digit;
    while (hex_value(*text, &digit)) {
        value = value * 16u + digit;
        if (value > FETCH_MAX_BODY) return 0xFFFF;
        text++;
    }
    return (uint16_t)value;
}

static uint8_t fetch_decode_chunked(char *body_start) {
    char *in = body_start;
    char *out = fetch_job.body;
    uint16_t out_len = 0;
    while (*in) {
        uint16_t chunk = hex_parse_line(in);
        if (chunk == 0xFFFF) return 0;
        char *line_end = find_text(in, "\r\n");
        if (!line_end) return 0;
        in = line_end + 2;
        if (chunk == 0) break;
        if (out_len + chunk > FETCH_MAX_BODY) return 0;
        kmemcpy(out + out_len, in, chunk);
        out_len = (uint16_t)(out_len + chunk);
        in += chunk;
        if (in[0] != '\r' || in[1] != '\n') return 0;
        in += 2;
    }
    fetch_job.body[out_len] = '\0';
    fetch_job.body_length = out_len;
    return 1;
}

static uint8_t fetch_parse_response(void) {
    fetch_job.response[fetch_job.response_length] = '\0';
    char *body_start = find_text(fetch_job.response, "\r\n\r\n");
    if (!body_start) { fetch_error("bad HTTP response"); return 0; }
    *body_start = '\0';
    body_start += 4;
    if (!text_starts(fetch_job.response, "HTTP/1.")) { fetch_error("bad HTTP status"); return 0; }
    uint16_t status = decimal_parse(fetch_job.response + 9);
    if (status >= 300 && status < 400 && fetch_job.redirects < 3) {
        char *location = find_header(fetch_job.response, "Location");
        if (!location) { fetch_error("redirect without Location"); return 0; }
        while (*location == ' ') location++;
        char next_url[192];
        uint16_t i = 0;
        while (location[i] && location[i] != '\r' && location[i] != '\n' && i + 1 < sizeof(next_url)) { next_url[i] = location[i]; i++; }
        next_url[i] = '\0';
        uint8_t redirects = (uint8_t)(fetch_job.redirects + 1);
        fetch_job.state = FETCH_DONE;
        if (!fetch_start_url(next_url, redirects)) return 0;
        return 0;
    }
    if (status < 200 || status >= 300) { fetch_error("HTTP status error"); return 0; }
    char *transfer = find_header(fetch_job.response, "Transfer-Encoding");
    if (transfer && text_starts_ci(transfer, " chunked")) return fetch_decode_chunked(body_start);
    char *cl = find_header(fetch_job.response, "Content-Length");
    uint16_t body_len = (uint16_t)kstrlen(body_start);
    if (cl) {
        uint16_t expected = decimal_parse(cl);
        if (expected > FETCH_MAX_BODY) { fetch_error("response too large"); return 0; }
        body_len = expected;
    }
    if (body_len > FETCH_MAX_BODY) { fetch_error("response too large"); return 0; }
    kmemcpy(fetch_job.body, body_start, body_len);
    fetch_job.body[body_len] = '\0';
    fetch_job.body_length = body_len;
    return 1;
}

static uint8_t fetch_start_url(const char *url, uint8_t redirects) {
    if (!rtl_found) { fetch_error("network adapter not found"); return 0; }
    if (fetch_job.state != FETCH_IDLE && fetch_job.state != FETCH_DONE && fetch_job.state != FETCH_ERROR) return 0;
    tcp_reset();
    tls_reset_state();
    kmemset(&dns_query, 0, sizeof(dns_query));
    kmemset(&fetch_job, 0, sizeof(fetch_job));
    fetch_job.redirects = redirects;
    fetch_job.state = FETCH_DNS;
    kstrcpy(fetch_job.url, url, sizeof(fetch_job.url));
    if (!parse_url(url)) { fetch_error("bad URL"); return 0; }
    if (fetch_job.https && !rtc_ready()) { fetch_error("RTC time unavailable"); return 0; }
    if (parse_ip(fetch_job.host, fetch_job.ip)) {
        tcp_connect(fetch_job.ip, fetch_job.port);
        fetch_job.state = FETCH_CONNECT;
    } else dns_start(fetch_job.host);
    return 1;
}

uint8_t net_fetch_start(const char *url) {
    return fetch_start_url(url, 0);
}

void net_fetch_cancel(void) {
    if (tcp_conn.state == TCP_ESTABLISHED) tcp_send_flags(&tcp_conn, 0x11, 0, 0);
    tcp_reset();
    tls_reset_state();
    fetch_error("fetch cancelled");
}

static void fetch_poll_dns(void) {
    dns_poll();
    if (dns_query.state == DNS_DONE) {
        kmemcpy(fetch_job.ip, dns_query.ip, 4);
        tcp_connect(fetch_job.ip, fetch_job.port);
        fetch_job.state = FETCH_CONNECT;
    } else if (dns_query.state == DNS_ERROR) fetch_error("DNS failed");
}

static void fetch_poll_tcp(void) {
    tcp_poll();
    if (tcp_conn.state == TCP_ERROR) { fetch_error("TCP failed"); return; }
    if (fetch_job.state == FETCH_CONNECT && tcp_conn.state == TCP_ESTABLISHED) {
        fetch_build_request();
        if (fetch_job.https && !tls_start()) return;
        fetch_job.state = FETCH_SEND;
    }
    if (fetch_job.https) {
        tls_pump();
        return;
    }
    if (fetch_job.state == FETCH_SEND && !fetch_job.request_sent) {
        if (tcp_send_data((const uint8_t*)fetch_job.request, (uint16_t)kstrlen(fetch_job.request))) fetch_job.request_sent = 1;
    }
    if (fetch_job.state == FETCH_SEND && fetch_job.request_sent && tcp_conn.tx_length == 0) fetch_job.state = FETCH_RECV;
    if (fetch_job.state == FETCH_RECV) {
        uint8_t temp[256];
        uint16_t n;
        while ((n = tcp_read(temp, sizeof(temp))) != 0) {
            if (!fetch_append_response(temp, n)) return;
        }
        if (tcp_conn.state == TCP_DONE) {
            if (fetch_parse_response()) fetch_job.state = FETCH_DONE;
        }
    }
}

void net_fetch_poll(void) {
    if (fetch_job.state == FETCH_DNS) fetch_poll_dns();
    else if (fetch_job.state == FETCH_CONNECT || fetch_job.state == FETCH_SEND || fetch_job.state == FETCH_RECV) fetch_poll_tcp();
}

net_fetch_status_t net_fetch_status(void) {
    if (fetch_job.state == FETCH_DONE) return NET_FETCH_DONE;
    if (fetch_job.state == FETCH_ERROR) return NET_FETCH_ERROR;
    if (fetch_job.state == FETCH_IDLE) return NET_FETCH_IDLE;
    return NET_FETCH_BUSY;
}

const char *net_fetch_body(void) { return fetch_job.body; }
uint16_t net_fetch_body_length(void) { return fetch_job.body_length; }
const char *net_fetch_error(void) { return fetch_job.error; }

static void handle_frame(const uint8_t *frame, uint16_t length) {
    if (length < 14) return;
    uint16_t type = rd16(frame + 12);
    if (type == ETH_ARP && length >= 42 && frame[20] == 0 && frame[21] == 2 && ip_equal(frame + 28, gateway_ip)) {
        copy_mac(gateway_mac, frame + 22);
        gateway_known = 1;
        if (ping_pending) { ping_pending = 0; send_ping(); }
        return;
    }
    if (type != ETH_IPV4 || length < 34) return;
    const uint8_t *ip = frame + 14;
    uint8_t ihl = (uint8_t)((ip[0] & 0x0F) * 4);
    if ((ip[0] >> 4) != 4 || ihl < 20 || length < 14 + ihl || !ip_equal(ip + 16, local_ip)) return;
    uint16_t total = rd16(ip + 2);
    if (total < ihl || total + 14 > length) return;
    const uint8_t *payload = ip + ihl;
    uint16_t payload_len = (uint16_t)(total - ihl);
    if (ip[9] == IP_ICMP && payload_len >= 8) {
        if (payload[0] == 0 && rd16(payload + 4) == 0x4D4F) ping_reply = 1;
    } else if (ip[9] == IP_UDP && payload_len >= 8) {
        uint16_t sport = rd16(payload);
        uint16_t dport = rd16(payload + 2);
        uint16_t udp_len = rd16(payload + 4);
        if (udp_len >= 8 && udp_len <= payload_len) dns_handle(payload + 8, (uint16_t)(udp_len - 8), sport, dport);
    } else if (ip[9] == IP_TCP) tcp_handle(ip, payload, payload_len);
}

void net_init(void) {
    for (uint16_t bus = 0; bus < 256 && !rtl_found; bus++)
        for (uint8_t dev = 0; dev < 32; dev++)
            if (pci_read((uint8_t)bus, dev, 0, 0) == RTL_VENDOR_DEVICE) {
                uint32_t command = pci_read((uint8_t)bus, dev, 0, 4);
                pci_write((uint8_t)bus, dev, 0, 4, command | 7);
                uint32_t bar = pci_read((uint8_t)bus, dev, 0, 0x10);
                rtl_base = (uint16_t)(bar & 0xFFFC);
                rtl_found = rtl_base != 0;
            }
    if (!rtl_found) return;
    outb(rtl_base + RTL_CMD, 0x10);
    for (uint32_t i = 0; i < 100000; i++) if (!(inb(rtl_base + RTL_CMD) & 0x10)) break;
    for (uint8_t i = 0; i < 6; i++) local_mac[i] = inb(rtl_base + RTL_IDR0 + i);
    outl(rtl_base + RTL_RBSTART, (uint32_t)rx_buffer);
    outw(rtl_base + RTL_CAPR, 0);
    outl(rtl_base + RTL_RCR, 0x0000F);
    outw(rtl_base + RTL_IMR, 0);
    outb(rtl_base + RTL_CMD, 0x0C);
    rx_offset = 0;
    fetch_job.state = FETCH_IDLE;
    tcp_reset();
    send_arp_request(gateway_ip);
}

void net_poll(void) {
    if (!rtl_found) return;
    while ((inb(rtl_base + RTL_CMD) & 1) == 0) {
        uint16_t status = *(uint16_t*)(rx_buffer + rx_offset);
        uint16_t length = *(uint16_t*)(rx_buffer + rx_offset + 2);
        if ((status & 1) && length >= 4 && length <= RTL_RX_BUF) handle_frame(rx_buffer + rx_offset + 4, (uint16_t)(length - 4));
        uint16_t next = (uint16_t)((rx_offset + length + 4 + 3) & ~3);
        if (next >= RTL_RX_BUF) next -= RTL_RX_BUF;
        rx_offset = next;
        outw(rtl_base + RTL_CAPR, (uint16_t)(rx_offset - 16));
    }
}

void net_ping_gateway(void) {
    kmemcpy(ping_target, gateway_ip, 4);
    ping_reply = 0;
    ping_pending = 1;
    if (!rtl_found) return;
    if (gateway_known) { ping_pending = 0; send_ping(); }
    else send_arp_request(gateway_ip);
}

uint8_t net_ping_ip(const char *address) {
    if (!parse_ip(address, ping_target)) return 0;
    ping_reply = 0;
    ping_pending = 1;
    if (!rtl_found) return 1;
    if (gateway_known) { ping_pending = 0; send_ping(); }
    else send_arp_request(gateway_ip);
    return 1;
}

uint8_t net_ready(void) { return rtl_found; }
uint8_t net_gateway_known(void) { return gateway_known; }
uint8_t net_ping_ok(void) { return ping_reply; }
