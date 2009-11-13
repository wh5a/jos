// See COPYRIGHT for copyright information.

#ifndef JOS_INC_NS_H
#define JOS_INC_NS_H

#include <inc/types.h>
#include <lwip/sockets.h>

// Definitions for requests from clients to network server

#define NSREQ_ACCEPT	1
#define NSREQ_BIND	2
#define NSREQ_SHUTDOWN	3
#define NSREQ_CLOSE	4
#define NSREQ_CONNECT	5
#define NSREQ_LISTEN	6
#define NSREQ_RECV	7
#define NSREQ_SEND	8
#define NSREQ_SOCKET	9

// The following two messages pass a page containing a struct jif_pkt
#define NSREQ_INPUT	10
#define NSREQ_OUTPUT	11

// The following message passes no page
#define NSREQ_TIMER	12

struct Nsreq_accept {
    int req_s;
};

struct Nsret_accept {
    struct sockaddr ret_addr;
    socklen_t ret_addrlen;
};

struct Nsreq_bind {
    int req_s;
    struct sockaddr req_name;
    socklen_t req_namelen;
};

struct Nsreq_shutdown {
    int req_s;
    int req_how;
};

struct Nsreq_close {
    int req_s;
};

struct Nsreq_connect {
    int req_s;
    struct sockaddr req_name;
    socklen_t req_namelen;
};

struct Nsreq_listen {
    int req_s;
    int req_backlog;
};

struct Nsreq_recv {
    int req_s;
    int req_len;
    unsigned int req_flags;
};

struct Nsreq_send {
    int req_s;
    int req_size;
    unsigned int req_flags;
    char req_dataptr[0];
};

struct Nsreq_socket {
    int req_domain;
    int req_type;
    int req_protocol;
};

struct jif_pkt {
    int jp_len;
    char jp_data[0];
};

#endif // !JOS_INC_NS_H
