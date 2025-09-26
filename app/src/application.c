
#include <application.h>
#include <logger.h>

#define STACKSIZE 256
#define PORT_MSG_WORDS 4U   /* 2 words meta + 2 words payload */
#define PORT_CAPACITY 16

/* tasks */
RK_DECLARE_TASK(serverHandle, ServerTask,    stack1, STACKSIZE)
RK_DECLARE_TASK(clientHandle, ClientTask,    stack2, STACKSIZE)

/* port */
static RK_PORT serverPort;
RK_DECLARE_PORT_BUF(portBuf, PORT_MSG_WORDS, PORT_CAPACITY)

/* 4-word message format; first two are reserved
for senderID and reply address */
typedef RK_PORT_MESG_4WORD RpcMsg;

static inline UINT crc32(const VOID *data, ULONG size)
{
    const BYTE *p = (const BYTE*) data;
    UINT crc = 0xFFFFFFFF;
    for (ULONG i = 0; i < size; ++i)
    {
        crc ^= p[i];
        for (BYTE j = 0; j < 8; ++j)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

/* some primes */
static BYTE seed = 0x5A;
static inline BYTE xorshift8(void)
{
    BYTE x = seed;
    x ^= x << 3;
    x ^= x >> 5;
    x ^= x << 7;
    seed = x;
    return (x);
}

VOID kApplicationInit(void)
{
    kassert(!kCreateTask(&serverHandle, ServerTask, RK_NO_ARGS,
                         "Server", stack1, STACKSIZE, 1, RK_PREEMPT));
    kassert(!kCreateTask(&clientHandle, ClientTask, RK_NO_ARGS,
                         "Client", stack2, STACKSIZE, 2, RK_PREEMPT));

    /* init the port; owner (server) is declared at creation */
    kassert(!kPortInit(&serverPort, portBuf, PORT_MSG_WORDS, PORT_CAPACITY,
                       serverHandle));

    logInit();
}

VOID ServerTask(VOID *args)
{
    RK_UNUSEARGS;
    RpcMsg msg;
    while(1)
    {
        /* receive next request; server may adopt client priority here */
        kassert(!kPortRecv(&serverPort, &msg, RK_WAIT_FOREVER));
        
        BYTE  *vector = (BYTE*) msg.payload[0];
        ULONG   size  =          msg.payload[1];
        UINT    crc   = crc32(vector, size);

        logPost("[SERVER] Will Reply CRC=0x%04X | Eff Prio=%d | Real Prio=%d",
                crc, runPtr->priority, runPtr->prioReal);

        /* must end with kPortReplyDone */
        kassert(!kPortReplyDone(&serverPort, (ULONG const*)&msg, crc));
    
        logPost("[SERVER] Finished. | Eff Prio: %d | Real Prio: %d", runPtr->priority, runPtr->prioReal);    
    }
}

VOID ClientTask(VOID *args)
{
    RK_UNUSEARGS;
    static BYTE vec[8];
    for (UINT i = 0; i < 8; ++i) 
        vec[i] = xorshift8();

    RK_MAILBOX replyBox;  /* 1-word mailbox reused across calls */
    kMailboxInit(&replyBox);

    RpcMsg msg = {0};
    msg.payload[0] = (ULONG) vec;  /* pointer as one word */
    msg.payload[1] = 8;            /* number of bytes */

    UINT reply = 0;
    while(1)
    {
        /* Send-Receive: a call */
        UINT want = crc32(vec, 8);
        kassert(!kPortSendRecv(&serverPort, (ULONG*)&msg, &replyBox, &reply,
                               RK_WAIT_FOREVER));
        logPost("[CLIENT] Need=0x%04X | Recvd=0x%04X", want, reply);
        /* if reply is correct, generate a new payload */
        if (want == reply)
            for (UINT i = 0; i < 8; ++i) vec[i] = xorshift8();
        kSleepPeriod(1000);
    }
}
