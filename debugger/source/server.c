// golden
// 6/12/2018
//

#include "server.h"
#include "paramdict.h"
#include "b64.h"

char *(*strtok)(char *str, const char *delimiters);
unsigned long long int (*strtoull)(const char *str, char **endptr, int base);

struct api_operation {
    char name[32];
    int (*handler)(int sock, struct paramdict *);
};

const char *status_to_str(int status) {
    switch(status) {
        case 200:
            return "OK.";
        case 404:
            return "Not Found.";
        case 405:
            return "Method Not Allowed.";
    }

    return "(null)";
}

void send_response(int sock, int status, char *body) {
    char header[1024];
    char *resp;
    int size;

    snprintf(header, sizeof(header), "HTTP/1.1 %i %s\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\nContent-Length: %i\r\n", status, status_to_str(status), body ? strlen(body) : 0);
    
    if(body) {
        size = strlen(header) + 2 + strlen(body);
    } else {
        size = strlen(header) + 2;
    }
    
    resp = (char *)pfmalloc(size + 1); // plus 1 for null terminator
    strcpy(resp, header);
    strcat(resp, "\r\n");
    
    if(body) {
        strcat(resp, body);
    }

    sceNetSend(sock, resp, size, 0);

    uprintf("sent response %i content-length %i", status, size);

    free(resp);
}

int handle_list(int sock, struct paramdict *params) {
    int i;

    struct proc_list_entry *plist;
    uint64_t numprocs = 0;

    if(sys_proc_list(NULL, &numprocs)) {
        return 1;
    }

    if(!numprocs) {
        return 1;
    }

    plist = (struct proc_list_entry *)pfmalloc(sizeof(struct proc_list_entry) * numprocs);
    memset(plist, 0, sizeof(struct proc_list_entry) * numprocs);
    if(sys_proc_list(plist, &numprocs)) {
        return 1;
    }

    char scratch[1024];
    int size = 8192;
    int cursize = 0;
    char *json = (char *)pfmalloc(size);
    memset(json, 0, size);
    strcat(json, "[ ");

    for(i = 0; i < numprocs; i++) {
        // build json for entry
        snprintf(scratch, sizeof(scratch), "{ \"name\": \"%s\", \"pid\": %i }%s", plist[i].p_comm, plist[i].pid, (i == (numprocs - 1)) ? "" : ",");

        cursize += strlen(scratch) + 1;
        if(cursize >= size - 1) {
            size += 4096;
            json = realloc(json, size);
        }

        strcat(json, scratch);
        memset(scratch, 0, sizeof(scratch));
    }

    char *end = " ]";
    cursize += strlen(end) + 1;
    if(cursize >= size - 1) {
        size += 4096;
        json = realloc(json, size);
    }
    strcat(json, end);

    send_response(sock, 200, json);
    
    free(plist);
    free(json);
   
    return 0;
}

int handle_info(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }
    
    struct sys_proc_info_args args;
    memset(&args, 0, sizeof(args));
    if(sys_proc_cmd(pid, SYS_PROC_INFO, &args)) {
        return 1;
    }

    // just allocate a scratch amount
    char *json = (char *)malloc(4096);

    char *version = "";
    

    snprintf(json, 4096, "{ \"name\": \"%s\", \"version\": \"%s\", \"path\": \"%s\", \"titleid\": \"%s\", \"contentid\": \"%s\" }", args.name, version, args.path, args.titleid, args.contentid);   

    send_response(sock, 200, json);

    free(json);

    return 0;
}



///////////////////////
int handle_mapping(int sock, struct paramdict *params) {
    int i;

    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    struct sys_proc_vm_map_args args;
    memset(&args, 0, sizeof(args));

    if(sys_proc_cmd(pid, SYS_PROC_VM_MAP, &args)) {
        return 1;
    }

    args.maps = (struct proc_vm_map_entry *)pfmalloc(sizeof(struct proc_vm_map_entry) * args.num);
    memset(args.maps, 0, sizeof(struct proc_vm_map_entry) * args.num);
    if(sys_proc_cmd(pid, SYS_PROC_VM_MAP, &args)) {
        free(args.maps);
        return 1;
    }

    char scratch[1024];
    int size = 8192;
    int cursize = 0;
    char *json = (char *)pfmalloc(size);
    memset(json, 0, size);
    strcat(json, "[ ");

    for(i = 0; i < args.num; i++) {
        // build json for entry
        struct proc_vm_map_entry *p = &args.maps[i];
        snprintf(scratch, sizeof(scratch), "{ \"name\": \"%s\", \"start\": %lli, \"end\": %lli, \"offset\": %lli, \"prot\": %i }%s", p->name, p->start, p->end, p->offset, p->prot, (i == (args.num - 1)) ? "" : ",");

        cursize += strlen(scratch) + 1;
        if(cursize >= size - 1) {
            size += 4096;
            json = realloc(json, size);
        }

        strcat(json, scratch);
        memset(scratch, 0, sizeof(scratch));
    }

    char *end = " ]";
    cursize += strlen(end) + 1;
    if(cursize >= size - 1) {
        size += 4096;
        json = realloc(json, size);
    }
    strcat(json, end);

    send_response(sock, 200, json);

    free(args.maps);
    free(json);

    return 0;
}


///////////////////////////////

int handle_notify(int sock, struct paramdict *params) {
    char *message = paramdict_search(params, "message");
    if(!message) {
        return 1;
    }

    char *messageType = paramdict_search(params, "messageType");
    if(!messageType) {
        return 1;
    }

    int type = strtoull(messageType, NULL, 0);
    if(errno) {
        return 1;
    }

       uprintf("type = %i\n", type);

    unsigned char *rawdata = b64_decode(message, strlen(message));
     
     uprintf("raw = %s\n", rawdata);

    sceSysUtilSendSystemNotificationWithText(type, rawdata);


    send_response(sock, 200, NULL);

    free(rawdata);

    return 0;
}


////////////////////////////////////////



int handle_write(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    char *saddress = paramdict_search(params, "address");
    if(!saddress) {
        return 1;
    }

    uint64_t address = strtoull(saddress, NULL, 0);
    if(errno) {
        return 1;
    }
    
    char *slength = paramdict_search(params, "length");
    if(!slength) {
        return 1;
    }

    uint64_t length = strtoull(slength, NULL, 0);
    if(errno) {
        return 1;
    }

    char *data = paramdict_search(params, "data");
    if(!data) {
        return 1;
    }

    unsigned char *rawdata = b64_decode(data, strlen(data));

    if(sys_proc_rw(pid, address, rawdata, length, 1)) {
        free(rawdata);
        return 1;
    }

    send_response(sock, 200, NULL);

    free(rawdata);

    return 0;
}

int handle_read(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    char *saddress = paramdict_search(params, "address");
    if(!saddress) {
        return 1;
    }

    uint64_t address = strtoull(saddress, NULL, 0);
    if(errno) {
        return 1;
    }
    
    char *slength = paramdict_search(params, "length");
    if(!slength) {
        return 1;
    }

    uint64_t length = strtoull(slength, NULL, 0);
    if(errno) {
        return 1;
    }

    unsigned char *data = (unsigned char *)pfmalloc(length);
    
    if(sys_proc_rw(pid, address, data, length, 0)) {
        free(data);
        return 1;
    }

    char *b64data = b64_encode(data, length);

    send_response(sock, 200, b64data);

    free(data);
    free(b64data);

    return 0;
}

int handle_alloc(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }
    
    char *slength = paramdict_search(params, "length");
    if(!slength) {
        return 1;
    }

    uint64_t length = strtoull(slength, NULL, 0);
    if(errno) {
        return 1;
    }

    struct sys_proc_alloc_args args;
    args.address = 0;
    args.length = length;

    if(sys_proc_cmd(pid, SYS_PROC_ALLOC, &args)) {
        return 1;
    }

    char scratch[512];
    snprintf(scratch, sizeof(scratch), "{ \"address\": %i }", args.address);
    send_response(sock, 200, scratch);

    return 0;
}

int handle_free(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    char *saddress = paramdict_search(params, "address");
    if(!saddress) {
        return 1;
    }

    uint64_t address = strtoull(saddress, NULL, 0);
    if(errno) {
        return 1;
    }
    
    char *slength = paramdict_search(params, "length");
    if(!slength) {
        return 1;
    }

    uint64_t length = strtoull(slength, NULL, 0);
    if(errno) {
        return 1;
    }

    struct sys_proc_free_args args;
    args.address = address;
    args.length = length;

    if(sys_proc_cmd(pid, SYS_PROC_FREE, &args)) {
        return 1;
    }

    send_response(sock, 200, NULL);

    return 0;
}

int handle_pause(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    kill(pid, 17); // SIGSTOP 17

    send_response(sock, 200, NULL);

    return 0;
}

int handle_resume(int sock, struct paramdict *params) {
    char *spid = paramdict_search(params, "pid");
    if(!spid) {
        return 1;
    }

    int pid = strtoull(spid, NULL, 0);
    if(errno) {
        return 1;
    }

    kill(pid, 19); // SIGCONT 19

    send_response(sock, 200, NULL);

    return 0;
}

struct api_operation operations[] = {
    { "list", handle_list },
    { "info", handle_info },
    { "mapping", handle_mapping },
    { "write", handle_write },
    { "read", handle_read },
    { "alloc", handle_alloc },
    { "free", handle_free },
    { "pause", handle_pause },
    { "resume", handle_resume },
    { "notify", handle_notify }
};

int handle_operation(int sock, char *operation, struct paramdict *params) {
    int i;

    for(i = 0; ; i++) {
        struct api_operation *oper = &operations[i];
        if(!oper->handler) {
            break;
        }

        if(!strcmp(oper->name, operation)) {
            uprintf("dispatching %s...", operation);
            return oper->handler(sock, params);
        }
    }

    return 1;
}

int handle_request(int sock) {
    char *buffer;
    int buffersize;
    int offset;
    int recvsize;
    int shouldcontinue;
    int i;

    offset = 0;
    buffersize = 4096;
    buffer = (char *)pfmalloc(buffersize);
    memset(buffer, 0, buffersize);

    shouldcontinue = 1;
    while(1) {
        recvsize = sceNetRecv(sock, buffer + offset, buffersize - offset, 0);

        if(recvsize) {
            shouldcontinue = 1;

            // search for \r\n\r\n which tells us it is the end
            for(i = 0; i < buffersize; i++) {
                if(!strncmp(buffer + i, "\r\n\r\n", 4)) {
                    shouldcontinue = 0;
                    break;
                }
            }

            if(shouldcontinue) {
                buffersize += 4096;
                offset += recvsize;
                buffer = (char *)realloc(buffer, buffersize);
            } else {
                shouldcontinue = 1;
                break;
            }
        } else {
            shouldcontinue = 0;
            break;
        }

    }

    struct paramdict *pd = paramdict_alloc();

    if(shouldcontinue) {
        // we only handle GET requests with parameters inside the url
        if(strncmp(buffer, "GET", 3)) {
            send_response(sock, 405, NULL);
            goto finish;
        }

        // the handling of this input will destroy the buffer's structure

        // break first line
        *strstr(buffer, "\r\n") = 0;

        // break second space
        *strstr(strstr(buffer, " ") + 1, " ") = 0;

        char *path = buffer + 4 + 1; // + 1 skip past the 'GET /'

        char operation[32];
        memset(operation, 0, sizeof(operation));

        char *qmark = strstr(path, "?");
        if(qmark) {
            strncpy(operation, path, qmark - path);
            
            char *params = path + strlen(operation) + 1;
            char *p = strtok(params, "&");
            while(p != NULL) {
                char *equalsign = strstr(p, "=");
                *equalsign = 0;
                paramdict_add(pd, p, equalsign + 1);

                p = strtok(NULL, "&");
            }
        } else {
            strncpy(operation, path, sizeof(operation));
        }
        
        uprintf("request path: %s", path);

        if(handle_operation(sock, operation, pd)) {
            send_response(sock, 404, NULL);
            goto finish;
        }
    }

finish:
    free(buffer);
    paramdict_free(pd);

    return 0;
}

int resolve() {
    int libc = sceKernelLoadStartModule("libSceLibcInternal.sprx", 0, NULL, 0, 0, 0);
    
    RESOLVE(libc, strtok);
    RESOLVE(libc, strtoull);

    return 0;
}

int start_http_server() {
    struct sockaddr_in server;
    struct sockaddr_in client;
    unsigned int len = sizeof(client);
    int serv, fd, r;

    uprintf("ps4 trainer http server");

    if(resolve()) {
        return 1;
    }

    // server structure
    server.sin_len = sizeof(server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = IN_ADDR_ANY;
    server.sin_port = sceNetHtons(SERVER_PORT);
    memset(server.sin_zero, NULL, sizeof(server.sin_zero));

    // start up server
    serv = sceNetSocket("httpmodsrv", AF_INET, SOCK_STREAM, 0);
    if(serv < 0) {
        uprintf("could not create socket!");
        return 1;
    }

    r = sceNetBind(serv, (struct sockaddr *)&server, sizeof(server));
    if(r) {
        uprintf("bind failed!");
        return 1;
    }

    r = sceNetListen(serv, 32);
    if(r) {
        uprintf("bind failed!");
        return 1;
    }

    while(1) {
        scePthreadYield();

        errno = NULL;
        fd = sceNetAccept(serv, (struct sockaddr *)&client, &len);
        if(fd > -1 && !errno) {
            uprintf("accepted a new client");

            if(handle_request(fd)) {
                uprintf("error handling client");
                break;
            }

            sceNetSocketClose(fd);
        }

        sceKernelUsleep(50000);
    }

    sceNetSocketClose(serv);

    return 0;
}
