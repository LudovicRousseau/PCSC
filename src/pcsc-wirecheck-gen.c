/*
 * Copyright (C) 2007
 *  Jacob Berkman
 * Copyright (C) 2007-2010
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

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
    CHECK_DEFINE (MAX_READERNAME);
    CHECK_DEFINE (MAX_ATR_SIZE);
    CHECK_DEFINE (MAX_BUFFER_SIZE);

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
    CHECK_VALUE (CMD_VERSION);
    CHECK_VALUE (CMD_GET_READERS_STATE);
    CHECK_VALUE (CMD_WAIT_READER_STATE_CHANGE);
    CHECK_VALUE (CMD_STOP_WAITING_READER_STATE_CHANGE);
}

static void
check_types (void)
{
    COMMENT ("Types...");

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
    CHECK_MEMBER (establish_struct, hContext);
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
    CHECK_MEMBER (connect_struct, hCard);
    CHECK_MEMBER (connect_struct, dwActiveProtocol);
    CHECK_MEMBER (connect_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (reconnect_struct);
    CHECK_MEMBER (reconnect_struct, hCard);
    CHECK_MEMBER (reconnect_struct, dwShareMode);
    CHECK_MEMBER (reconnect_struct, dwPreferredProtocols);
    CHECK_MEMBER (reconnect_struct, dwInitialization);
    CHECK_MEMBER (reconnect_struct, dwActiveProtocol);
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
    CHECK_MEMBER (cancel_struct, hContext);
    CHECK_MEMBER (cancel_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (status_struct);
    CHECK_MEMBER (status_struct, hCard);
    CHECK_MEMBER (status_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (transmit_struct);
    CHECK_MEMBER (transmit_struct, hCard);
    CHECK_MEMBER (transmit_struct, ioSendPciProtocol);
    CHECK_MEMBER (transmit_struct, ioSendPciLength);
    CHECK_MEMBER (transmit_struct, cbSendLength);
    CHECK_MEMBER (transmit_struct, ioRecvPciProtocol);
    CHECK_MEMBER (transmit_struct, ioRecvPciLength);
    CHECK_MEMBER (transmit_struct, pcbRecvLength);
    CHECK_MEMBER (transmit_struct, rv);

    BLANK_LINE ();
    CHECK_STRUCT (control_struct);
    CHECK_MEMBER (control_struct, hCard);
    CHECK_MEMBER (control_struct, dwControlCode);
    CHECK_MEMBER (control_struct, cbSendLength);
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
    CHECK_MEMBER (pubReaderStatesList, readerName);
    CHECK_MEMBER (pubReaderStatesList, readerState);
    CHECK_MEMBER (pubReaderStatesList, readerSharing);
    CHECK_MEMBER (pubReaderStatesList, cardAtr);
    CHECK_MEMBER (pubReaderStatesList, cardAtrLength);
    CHECK_MEMBER (pubReaderStatesList, cardProtocol);
}

int
main(/*@unused@*/ int argc, /*@unused@*/ char **argv)
{
	(void)argc;
	(void)argv;

    printf ("#include <sys/types.h>\n"
            "#include <time.h>\n"
            "#include <stddef.h>\n\n"
            "#include \"PCSC/pcsclite.h\"\n"
            "#include \"pcscd.h\"\n"
            "#include \"readerfactory.h\"\n"
            "#include \"eventhandler.h\"\n"
            "#include \"winscard_msg.h\"\n\n"
            "#include \"lassert.h\"\n\n"
            "int pcsc_assert_wire_constants(void);\n"
            "int pcsc_assert_wire_constants(void)\n"
            "{\n");

    BLANK_LINE ();

    check_constants ();
    check_types ();

    BLANK_LINE ();

    printf ("return 0;\n");
    printf ("}\n");

    return 0;
}
