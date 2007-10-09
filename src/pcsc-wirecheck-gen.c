#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stddef.h>

#include "PCSC/pcsclite.h"
#include "pcscd.h"
#include "winscard_msg.h"
#include "readerfactory.h"
#include "eventhandler.h"

#define BLANK_LINE()                                            \
do {                                                            \
        printf("\n");                                           \
} while(0)

#define COMMENT(c)                                              \
do {                                                            \
        printf("        /* "c" */\n");                          \
} while(0)

#define STRINGIFY(a) #a


#define CHECK_CDEFINE(a)                                        \
        printf("        CLASSERT("#a" == "STRINGIFY(a) ");\n")

#define CHECK_CVALUE(a)                                         \
        printf("        CLASSERT("#a" == %lld);\n", (long long)a)

#define CHECK_DEFINE(a)                                         \
do {                                                            \
        printf("        LASSERTF("#a" == "STRINGIFY(a)          \
               ",\" found %%lld\\n\",\n                 "       \
               "(long long)"#a");\n");   \
} while(0)

#define CHECK_VALUE(a)                                          \
do {                                                            \
        printf("        LASSERTF("#a                            \
               " == %lld, \" found %%lld\\n\",\n                 "\
               "(long long)"#a");\n", (long long)a);            \
} while(0)

#define CHECK_VALUE_64(a)                                       \
do {                                                            \
        printf("        LASSERTF("#a                            \
               " == %lldULL, \" found %%lld\\n\",\n                 "\
               "(long long)"#a");\n", (long long)a);            \
} while(0)

#define CHECK_MEMBER_OFFSET(s,m)                                \
do {                                                            \
        CHECK_VALUE((int)offsetof(struct s, m));                \
} while(0)

#define CHECK_MEMBER_SIZEOF(s,m)                                \
do {                                                            \
        CHECK_VALUE((int)sizeof(((struct s *)0)->m));           \
} while(0)

#define CHECK_MEMBER(s,m)                                       \
do {                                                            \
        CHECK_MEMBER_OFFSET(s, m);                              \
                CHECK_MEMBER_SIZEOF(s, m);                      \
} while(0)

#define CHECK_STRUCT(s)                                         \
do {                                                            \
        COMMENT("Checks for struct "#s);                        \
                CHECK_VALUE((int)sizeof(struct s));             \
} while(0)

static void
check_constants (void)
{
    COMMENT ("Constants...");

    BLANK_LINE ();
    CHECK_DEFINE (PROTOCOL_VERSION_MAJOR);
    CHECK_DEFINE (PROTOCOL_VERSION_MINOR);

    BLANK_LINE ();
    CHECK_DEFINE (PCSCLITE_MSG_KEY_LEN);
    CHECK_DEFINE (PCSCLITE_MAX_MESSAGE_SIZE);

    BLANK_LINE ();
    CHECK_DEFINE (MAX_READERNAME);
    CHECK_DEFINE (MAX_ATR_SIZE);
    CHECK_DEFINE (MAX_BUFFER_SIZE);

    BLANK_LINE ();
    COMMENT ("enum pcsc_adm_commands");
    CHECK_VALUE (CMD_FUNCTION);
    CHECK_VALUE (CMD_FAILED);
    CHECK_VALUE (CMD_SERVER_DIED);
    CHECK_VALUE (CMD_CLIENT_DIED);
    CHECK_VALUE (CMD_READER_EVENT);
    CHECK_VALUE (CMD_SYN);
    CHECK_VALUE (CMD_ACK);
    CHECK_VALUE (CMD_VERSION);

    BLANK_LINE ();
    COMMENT ("enum pcsc_msg_commands");
    CHECK_VALUE (SCARD_ESTABLISH_CONTEXT);
    CHECK_VALUE (SCARD_RELEASE_CONTEXT);
    CHECK_VALUE (SCARD_LIST_READERS);
    CHECK_VALUE (SCARD_CONNECT);
    CHECK_VALUE (SCARD_RECONNECT);
    CHECK_VALUE (SCARD_DISCONNECT);
    CHECK_VALUE (SCARD_BEGIN_TRANSACTION);
    CHECK_VALUE (SCARD_END_TRANSACTION);
    CHECK_VALUE (SCARD_TRANSMIT);
    CHECK_VALUE (SCARD_CONTROL);
    CHECK_VALUE (SCARD_STATUS);
    CHECK_VALUE (SCARD_GET_STATUS_CHANGE);
    CHECK_VALUE (SCARD_CANCEL);
    CHECK_VALUE (SCARD_CANCEL_TRANSACTION);
    CHECK_VALUE (SCARD_GET_ATTRIB);
    CHECK_VALUE (SCARD_SET_ATTRIB);
}

static void
check_types (void)
{
    COMMENT ("Types...");

    BLANK_LINE ();
    CHECK_STRUCT (rxSharedSegment);
    CHECK_MEMBER (rxSharedSegment, mtype);
    CHECK_MEMBER (rxSharedSegment, user_id);
    CHECK_MEMBER (rxSharedSegment, group_id);
    CHECK_MEMBER (rxSharedSegment, command);
    CHECK_MEMBER (rxSharedSegment, date);
    CHECK_MEMBER (rxSharedSegment, key);
    CHECK_MEMBER (rxSharedSegment, data);

    BLANK_LINE ();
    CHECK_STRUCT (version_struct);
    CHECK_MEMBER (version_struct, major);
    CHECK_MEMBER (version_struct, minor);
    CHECK_MEMBER (version_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (client_struct);
    CHECK_MEMBER (client_struct, hContext);

    BLANK_LINE ();
    CHECK_STRUCT (establish_struct);
    CHECK_MEMBER (establish_struct, dwScope);
    CHECK_MEMBER (establish_struct, phContext);
    CHECK_MEMBER (establish_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (release_struct);
    CHECK_MEMBER (release_struct, hContext);
    CHECK_MEMBER (release_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (connect_struct);
    CHECK_MEMBER (connect_struct, hContext);
    CHECK_MEMBER (connect_struct, szReader);
    CHECK_MEMBER (connect_struct, dwShareMode);
    CHECK_MEMBER (connect_struct, dwPreferredProtocols);
    CHECK_MEMBER (connect_struct, phCard);
    CHECK_MEMBER (connect_struct, pdwActiveProtocol);
    CHECK_MEMBER (connect_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (reconnect_struct);
    CHECK_MEMBER (reconnect_struct, hCard);
    CHECK_MEMBER (reconnect_struct, dwShareMode);
    CHECK_MEMBER (reconnect_struct, dwPreferredProtocols);
    CHECK_MEMBER (reconnect_struct, dwInitialization);
    CHECK_MEMBER (reconnect_struct, pdwActiveProtocol);
    CHECK_MEMBER (reconnect_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (disconnect_struct);
    CHECK_MEMBER (disconnect_struct, hCard);
    CHECK_MEMBER (disconnect_struct, dwDisposition);
    CHECK_MEMBER (disconnect_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (begin_struct);
    CHECK_MEMBER (begin_struct, hCard);
    CHECK_MEMBER (begin_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (end_struct);
    CHECK_MEMBER (end_struct, hCard);
    CHECK_MEMBER (end_struct, dwDisposition);
    CHECK_MEMBER (end_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (cancel_struct);
    CHECK_MEMBER (cancel_struct, hCard);
    CHECK_MEMBER (cancel_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (status_struct);
    CHECK_MEMBER (status_struct, hCard);
    CHECK_MEMBER (status_struct, mszReaderNames);
    CHECK_MEMBER (status_struct, pcchReaderLen);
    CHECK_MEMBER (status_struct, pdwState);
    CHECK_MEMBER (status_struct, pdwProtocol);
    CHECK_MEMBER (status_struct, pbAtr);
    CHECK_MEMBER (status_struct, pcbAtrLen);
    CHECK_MEMBER (status_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (transmit_struct);
    CHECK_MEMBER (transmit_struct, hCard);
    CHECK_MEMBER (transmit_struct, pioSendPciProtocol);
    CHECK_MEMBER (transmit_struct, pioSendPciLength);
    CHECK_MEMBER (transmit_struct, pbSendBuffer);
    CHECK_MEMBER (transmit_struct, cbSendLength);
    CHECK_MEMBER (transmit_struct, pioRecvPciProtocol);
    CHECK_MEMBER (transmit_struct, pioRecvPciLength);
    CHECK_MEMBER (transmit_struct, pbRecvBuffer);
    CHECK_MEMBER (transmit_struct, pcbRecvLength);
    CHECK_MEMBER (transmit_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (control_struct);
    CHECK_MEMBER (control_struct, hCard);
    CHECK_MEMBER (control_struct, dwControlCode);
    CHECK_MEMBER (control_struct, pbSendBuffer);
    CHECK_MEMBER (control_struct, cbSendLength);
    CHECK_MEMBER (control_struct, pbRecvBuffer);
    CHECK_MEMBER (control_struct, cbRecvLength);
    CHECK_MEMBER (control_struct, dwBytesReturned);
    CHECK_MEMBER (control_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (getset_struct);
    CHECK_MEMBER (getset_struct, hCard);
    CHECK_MEMBER (getset_struct, dwAttrId);
    CHECK_MEMBER (getset_struct, cbAttrLen);
    CHECK_MEMBER (getset_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (pubReaderStatesList);
    CHECK_MEMBER (pubReaderStatesList, readerID);
    CHECK_MEMBER (pubReaderStatesList, readerName);
    CHECK_MEMBER (pubReaderStatesList, readerState);
    CHECK_MEMBER (pubReaderStatesList, readerSharing);
    CHECK_MEMBER (pubReaderStatesList, cardAtr);
    CHECK_MEMBER (pubReaderStatesList, cardAtrLength);
    CHECK_MEMBER (pubReaderStatesList, cardProtocol);
}

int
main(int argc, char **argv)
{
    printf ("#include <sys/types.h>\n"
            "#include <time.h>\n"
            "#include <stddef.h>\n\n"
            "#include \"PCSC/pcsclite.h\"\n"
            "#include \"pcscd.h\"\n"
            "#include \"readerfactory.h\"\n"
            "#include \"eventhandler.h\"\n"
            "#include \"winscard_msg.h\"\n\n"
            "#include \"lassert.h\"\n\n"
            "void pcsc_assert_wire_constants(void);\n"
            "void pcsc_assert_wire_constants(void)\n"
            "{\n");

    BLANK_LINE ();
 
    check_constants ();
    check_types ();

    BLANK_LINE ();

    printf ("}\n");

    return 0;
}
