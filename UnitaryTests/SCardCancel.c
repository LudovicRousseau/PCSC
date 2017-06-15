#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#include <winscard.h>
#endif

#define CHECK(f, rv) printf(f ":[0x%08lX] %s\n", rv, pcsc_stringify_error(rv))

#define GREEN "\33[32m"
#define BRIGHT_RED "\33[01;31m"
#define NORMAL "\33[0m"

static SCARDCONTEXT context;

static void *canceler_thread(void *arg) {
    LONG ret;

    getchar();

    printf("Calling SCardCancel...\n");
    ret = SCardCancel(context);
    CHECK("SCardCancel", ret);

    return NULL;
}

int main(void) {
    LONG ret;
	int delay = 3;

    ret = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &context);
    CHECK("SCardEstablishContext", ret);

    // Spawn a thread which waits for a key-press then cancels the operation.
    pthread_t thread;
    ret = pthread_create(&thread, NULL, canceler_thread, NULL);

	printf(GREEN "Press Enter to cancel within %d seconds\n" NORMAL, delay);

    // Set up the blocking call, and wait for cancel or timeout.
    printf("Entering blocking call\n");
    SCARD_READERSTATE reader_states[] = {
        {
            .szReader = "\\\\?PnP?\\Notification",
            .pvUserData = NULL,
            .dwCurrentState = SCARD_STATE_UNAWARE,
            .dwEventState = SCARD_STATE_UNAWARE,
        },
    };

    ret = SCardGetStatusChange(context, delay * 1000, reader_states, 1);
	CHECK("SCardGetStatusChange", ret);
    switch (ret) {
    case SCARD_S_SUCCESS:
        printf("Blocking call exited normally\n");
        break;

    case SCARD_E_CANCELLED:
		/* this should be the correct returned value */
        printf("Blocking call canceled\n");
        break;

    case SCARD_E_TIMEOUT:
        printf("Blocking call timed out\n");
        break;

    default:
        fprintf(stderr, "Failed to get status changes: %ld", ret);
        break;
    }
	if (SCARD_E_CANCELLED != ret)
		printf(BRIGHT_RED "ERROR: Something wrong happened!\n" NORMAL);
	else
		printf(GREEN "Good\n" NORMAL);

	ret = SCardReleaseContext(context);
	CHECK("SCardReleaseContext", ret);

    printf("Waiting thread...\n");
	pthread_join(thread, NULL);

    return 0;
}
