/**
 * @file        hsed_spc_sm2_stress.c
 * @brief       HSED SPC multi-channel SM2 signing performance test
 * @author      HSED
 * @date        2026/07/26
 * @version     1.0
 */

#include "generated/hsed_conn.h"
#include "generated/hsed_datatype.h"

#include "SpanCall.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HSED_STRESS_MAX_CHANNELS       8
#define HSED_STRESS_DEFAULT_CHANNELS   4
#define HSED_STRESS_DEFAULT_ITERATIONS 10
#define HSED_STRESS_DEFAULT_MESSAGE    32
#define HSED_STRESS_DEFAULT_PUBLIC_KEY_ID  11
#define HSED_STRESS_DEFAULT_PRIVATE_KEY_ID 12
#define HSED_STRESS_MAX_MESSAGE        4096
#define HSED_STRESS_USER_ID_LEN        16
#define HSED_STRESS_SIGNATURE_LEN      512
#define HSED_STRESS_SM2_PUBLIC_KEY_LEN 64
#define HSED_STRESS_SM2_SIGNATURE_LEN  64
#define HSED_STRESS_TIMEOUT_MS         30000

typedef struct THsedStressChannelTag THsedStressChannel;
struct THsedStressChannelTag
{
    int            channel_id;
    int            iterations;
    int            message_size;
    int            public_key_id;
    int            private_key_id;
    int            setup_result;
    int            success_count;
    int            failure_count;
    uint64_t       first_call_ns;
    uint64_t       last_call_ns;
    uint64_t       min_latency_ns;
    uint64_t       max_latency_ns;
    uint64_t       total_latency_ns;
    unsigned char  user_id[HSED_STRESS_USER_ID_LEN];
    unsigned char  message[HSED_STRESS_MAX_MESSAGE];
    unsigned char  sample_public_key[HSED_STRESS_SM2_PUBLIC_KEY_LEN];
    unsigned char  sample_signature[HSED_STRESS_SM2_SIGNATURE_LEN];
    int            sample_public_key_size;
    int            sample_signature_size;
    Bool           capture_sample;
    Bool           sample_ready;
};

static const unsigned char g_verify_user_id_[HSED_STRESS_USER_ID_LEN] =
{
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBB, 0xCC, 0xC0,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xBC, 0xCC
};

static pthread_mutex_t g_start_mutex_ = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_start_cond_ = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_print_mutex_ = PTHREAD_MUTEX_INITIALIZER;
static int g_ready_count_ = 0;
static Bool g_start_ = False;

static uint64_t L_MonotonicNs(void)
{
    uint64_t result = 0;
    struct timespec now = {0};

    if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)
    {
        result = ((uint64_t)now.tv_sec * 1000000000ULL) +
                 (uint64_t)now.tv_nsec;
    }
    return result;
}

static int L_ParsePositive(const char *text, int maximum, int *value)
{
    int result = -1;
    char *end = NULL;
    long parsed = 0;

    errno = 0;
    parsed = strtol(text, &end, 0);
    if ((errno == 0) && (end != text) && (*end == '\0') &&
        (parsed > 0) && (parsed <= maximum))
    {
        *value = (int)parsed;
        result = 0;
    }
    return result;
}

static int L_LoadRandom(unsigned char *buffer, size_t size)
{
    int result = -1;
    FILE *random_file = fopen("/dev/urandom", "rb");

    if (random_file != NULL)
    {
        if (fread(buffer, 1, size, random_file) == size)
        {
            result = 0;
        }
        (void)fclose(random_file);
    }
    return result;
}

static void L_PrintUserId(const THsedStressChannel *channel)
{
    int index;

    (void)pthread_mutex_lock(&g_print_mutex_);
    (void)printf("[CHANNEL %d] user_id=", channel->channel_id);
    for (index = 0; index < HSED_STRESS_USER_ID_LEN; index++)
    {
        (void)printf("%02X", channel->user_id[index]);
    }
    (void)printf("\n");
    (void)fflush(stdout);
    (void)pthread_mutex_unlock(&g_print_mutex_);
}

static void L_PrintCallResult(int channel_id,
                              int iteration,
                              int transport_result,
                              int business_result,
                              uint64_t elapsed_ns)
{
    double elapsed_us = (double)elapsed_ns / 1000.0;
    double calls_per_second = 0.0;

    if (elapsed_ns > 0)
    {
        calls_per_second = 1000000000.0 / (double)elapsed_ns;
    }

    (void)pthread_mutex_lock(&g_print_mutex_);
    (void)printf("SIGN,channel=%d,iteration=%d,transport=%d,business=%d,"
                 "latency_us=%.3f,calls_per_second=%.2f\n",
                 channel_id, iteration, transport_result, business_result,
                 elapsed_us, calls_per_second);
    (void)fflush(stdout);
    (void)pthread_mutex_unlock(&g_print_mutex_);
}

static int L_CommandCreate(void *connection,
                           int *business_result,
                           TInt64 *handle)
{
    int result = SC_rHandleInvalid;
    void *command = SCCmdCreate();

    if (command != NULL)
    {
        result = SCCmdBegin(command, connection, "HseCreate");
        if (result == SC_rSuccess)
        {
            result = SCCmdExecute(command, HSED_STRESS_TIMEOUT_MS);
            if (result == SC_rSuccess)
            {
                *business_result = SCCmdRetAsInt(command, NULL);
                *handle = SCCmdFieldAsInt64(command, 0, NULL);
            }
            SCCmdEnd(command);
        }
        SCCmdFree(command);
    }
    return result;
}

static int L_CommandDestroy(void *connection,
                            TInt64 handle,
                            int *business_result)
{
    int result = SC_rHandleInvalid;
    void *command = SCCmdCreate();

    if (command != NULL)
    {
        result = SCCmdBegin(command, connection, "HseDestroy");
        if (result == SC_rSuccess)
        {
            SCCmdParamByInt64(command, 0, handle);
            result = SCCmdExecute(command, HSED_STRESS_TIMEOUT_MS);
            if (result == SC_rSuccess)
            {
                *business_result = SCCmdRetAsInt(command, NULL);
            }
            SCCmdEnd(command);
        }
        SCCmdFree(command);
    }
    return result;
}

static int L_CommandSetUserId(void *connection,
                              TInt64 handle,
                              const unsigned char *user_id,
                              int *business_result)
{
    int result = SC_rHandleInvalid;
    void *command = SCCmdCreate();

    if (command != NULL)
    {
        result = SCCmdBegin(command, connection, "SetPropertyBytes");
        if (result == SC_rSuccess)
        {
            SCCmdParamByInt64(command, 0, handle);
            SCCmdParamByInt(command, 1, kHsedHsedPropertyId_USER_ID);
            SCCmdParamByStr(command, 2, (const char *)user_id,
                            HSED_STRESS_USER_ID_LEN);
            result = SCCmdExecute(command, HSED_STRESS_TIMEOUT_MS);
            if (result == SC_rSuccess)
            {
                *business_result = SCCmdRetAsInt(command, NULL);
            }
            SCCmdEnd(command);
        }
        SCCmdFree(command);
    }
    return result;
}

static int L_CommandSetInt(void *connection,
                           TInt64 handle,
                           int property_id,
                           int value,
                           int *business_result)
{
    int result = SC_rHandleInvalid;
    void *command = SCCmdCreate();

    if (command != NULL)
    {
        result = SCCmdBegin(command, connection, "SetPropertyInt");
        if (result == SC_rSuccess)
        {
            SCCmdParamByInt64(command, 0, handle);
            SCCmdParamByInt(command, 1, property_id);
            SCCmdParamByInt(command, 2, value);
            result = SCCmdExecute(command, HSED_STRESS_TIMEOUT_MS);
            if (result == SC_rSuccess)
            {
                *business_result = SCCmdRetAsInt(command, NULL);
            }
            SCCmdEnd(command);
        }
        SCCmdFree(command);
    }
    return result;
}

static int L_CommandGetInt(void *connection,
                           TInt64 handle,
                           int property_id,
                           int *business_result,
                           int *value)
{
    int result = SC_rHandleInvalid;
    void *command = SCCmdCreate();

    if (command != NULL)
    {
        result = SCCmdBegin(command, connection, "GetPropertyInt");
        if (result == SC_rSuccess)
        {
            SCCmdParamByInt64(command, 0, handle);
            SCCmdParamByInt(command, 1, property_id);
            result = SCCmdExecute(command, HSED_STRESS_TIMEOUT_MS);
            if (result == SC_rSuccess)
            {
                *business_result = SCCmdRetAsInt(command, NULL);
                *value = SCCmdFieldAsInt(command, 0, NULL);
            }
            SCCmdEnd(command);
        }
        SCCmdFree(command);
    }
    return result;
}

static int L_CommandSign(void *connection,
                         TInt64 handle,
                         unsigned char *message,
                         int message_size,
                         int *business_result,
                         unsigned char *signature,
                         int *signature_size,
                         unsigned int *r_size,
                         unsigned int *s_size)
{
    int result = SC_rHandleInvalid;
    int returned_size = 0;
    char *returned_signature = NULL;
    void *command = SCCmdCreate();

    if (command != NULL)
    {
        result = SCCmdBegin(command, connection, "Sign");
        if (result == SC_rSuccess)
        {
            SCCmdParamByInt64(command, 0, handle);
            SCCmdParamByInt(command, 1, kHsedHsedAlgorithm_SM2);
            SCCmdParamByStr(command, 2, (const char *)message, message_size);
            result = SCCmdExecute(command, HSED_STRESS_TIMEOUT_MS);
            if (result == SC_rSuccess)
            {
                *business_result = SCCmdRetAsInt(command, NULL);
                returned_signature =
                    SCCmdFieldAsStr(command, 0, &returned_size);
                if ((returned_signature == NULL) ||
                    (returned_size < 0) ||
                    (returned_size > *signature_size))
                {
                    result = SC_rFailure;
                }
                else
                {
                    memcpy(signature, returned_signature,
                           (size_t)returned_size);
                    *signature_size = returned_size;
                    *r_size = (unsigned int)SCCmdFieldAsInt(
                        command, 1, NULL);
                    *s_size = (unsigned int)SCCmdFieldAsInt(
                        command, 2, NULL);
                }
            }
            SCCmdEnd(command);
        }
        SCCmdFree(command);
    }
    return result;
}

static int L_CommandExportPublicKey(void *connection,
                                    TInt64 handle,
                                    int *business_result,
                                    unsigned char *public_key,
                                    int *public_key_size)
{
    int result = SC_rHandleInvalid;
    int returned_size = 0;
    char *returned_key = NULL;
    void *command = SCCmdCreate();

    if (command != NULL)
    {
        result = SCCmdBegin(command, connection, "ExportPublicKey");
        if (result == SC_rSuccess)
        {
            SCCmdParamByInt64(command, 0, handle);
            SCCmdParamByInt(command, 1, kHsedHsedAlgorithm_SM2);
            result = SCCmdExecute(command, HSED_STRESS_TIMEOUT_MS);
            if (result == SC_rSuccess)
            {
                *business_result = SCCmdRetAsInt(command, NULL);
                returned_key = SCCmdFieldAsStr(command, 0, &returned_size);
                if ((returned_key == NULL) ||
                    (returned_size != HSED_STRESS_SM2_PUBLIC_KEY_LEN) ||
                    (returned_size > *public_key_size))
                {
                    result = SC_rFailure;
                }
                else
                {
                    memcpy(public_key, returned_key, (size_t)returned_size);
                    *public_key_size = returned_size;
                }
            }
            SCCmdEnd(command);
        }
        SCCmdFree(command);
    }
    return result;
}

static int L_SetupChannel(THsedStressChannel *channel,
                          void **connection,
                          TInt64 *handle)
{
    int result = -1;
    int transport_result = SC_rFailure;
    int business_result = kHsedHsedResult_DEVICE_ERROR;

    *connection = HcHsedConn_Create(NULL);
    if (*connection == NULL)
    {
        (void)fprintf(stderr, "[CHANNEL %d] connection create failed\n",
                      channel->channel_id);
    }
    else
    {
        transport_result = HcHsedConn_Open(*connection);
        if (transport_result != SC_rSuccess)
        {
            (void)fprintf(stderr,
                          "[CHANNEL %d] connection open failed: %d\n",
                          channel->channel_id, transport_result);
        }
        else
        {
            transport_result =
                L_CommandCreate(*connection, &business_result, handle);
            if ((transport_result != SC_rSuccess) ||
                (business_result != kHsedHsedResult_OK))
            {
                (void)fprintf(stderr,
                              "[CHANNEL %d] HseCreate failed: "
                              "transport=%d business=%d\n",
                              channel->channel_id, transport_result,
                              business_result);
            }
            else
            {
                int actual_public_key_id = 0;
                int actual_private_key_id = 0;

                transport_result = L_CommandGetInt(
                    *connection, *handle,
                    kHsedHsedPropertyId_SM2_PUBLIC_KEY_ID,
                    &business_result, &actual_public_key_id);
                if ((transport_result == SC_rSuccess) &&
                    (business_result == kHsedHsedResult_OK))
                {
                    transport_result = L_CommandGetInt(
                        *connection, *handle,
                        kHsedHsedPropertyId_SM2_PRIVATE_KEY_ID,
                        &business_result, &actual_private_key_id);
                }
                if ((transport_result == SC_rSuccess) &&
                    (business_result == kHsedHsedResult_OK) &&
                    ((actual_public_key_id != channel->public_key_id) ||
                     (actual_private_key_id != channel->private_key_id)))
                {
                    (void)fprintf(
                        stderr,
                        "[CHANNEL %d] fixed SM2 key mismatch: "
                        "public=%d expected=%d private=%d expected=%d\n",
                        channel->channel_id,
                        actual_public_key_id, channel->public_key_id,
                        actual_private_key_id, channel->private_key_id);
                    business_result = kHsedHsedResult_OBJECT_STATE_ERROR;
                }
                if ((transport_result == SC_rSuccess) &&
                    (business_result == kHsedHsedResult_OK))
                {
                    transport_result = L_CommandSetInt(
                        *connection, *handle,
                        kHsedHsedPropertyId_SM2_HASH_ALGORITHM,
                        kHsedHsedAlgorithm_SM3, &business_result);
                }
                if ((transport_result == SC_rSuccess) &&
                    (business_result == kHsedHsedResult_OK))
                {
                    transport_result = L_CommandSetInt(
                        *connection, *handle,
                        kHsedHsedPropertyId_SM2_SIGN_INPUT_MODE,
                        kHsedHsedSignInputMode_MESSAGE, &business_result);
                }
                if ((transport_result == SC_rSuccess) &&
                    (business_result == kHsedHsedResult_OK))
                {
                    transport_result = L_CommandSetUserId(
                        *connection, *handle, channel->user_id,
                        &business_result);
                }
                if ((transport_result == SC_rSuccess) &&
                    (business_result == kHsedHsedResult_OK))
                {
                    if (channel->capture_sample == True)
                    {
                        channel->sample_public_key_size =
                            (int)sizeof(channel->sample_public_key);
                        transport_result = L_CommandExportPublicKey(
                            *connection, *handle, &business_result,
                            channel->sample_public_key,
                            &channel->sample_public_key_size);
                    }
                    if ((transport_result == SC_rSuccess) &&
                        (business_result == kHsedHsedResult_OK))
                    {
                        result = 0;
                    }
                }
                else
                {
                    (void)fprintf(stderr,
                                  "[CHANNEL %d] setup failed: "
                                  "transport=%d business=%d\n",
                                  channel->channel_id, transport_result,
                                  business_result);
                }
            }
        }
    }
    return result;
}

static void L_CleanupChannel(void *connection, TInt64 handle)
{
    int business_result = kHsedHsedResult_DEVICE_ERROR;

    if ((connection != NULL) && (handle != 0))
    {
        (void)L_CommandDestroy(connection, handle, &business_result);
    }
    if (connection != NULL)
    {
        (void)HcHsedConn_Close(connection);
        HcHsedConn_Free(connection);
    }
}

static void L_WaitForStart(void)
{
    (void)pthread_mutex_lock(&g_start_mutex_);
    g_ready_count_++;
    (void)pthread_cond_signal(&g_start_cond_);
    while (g_start_ == False)
    {
        (void)pthread_cond_wait(&g_start_cond_, &g_start_mutex_);
    }
    (void)pthread_mutex_unlock(&g_start_mutex_);
}

static void *L_ChannelThread(void *argument)
{
    int iteration;
    int transport_result;
    int business_result;
    int signature_size;
    unsigned int r_size;
    unsigned int s_size;
    uint64_t begin_ns;
    uint64_t end_ns;
    uint64_t elapsed_ns;
    TInt64 handle = 0;
    void *connection = NULL;
    unsigned char signature[HSED_STRESS_SIGNATURE_LEN] = {0};
    THsedStressChannel *channel = (THsedStressChannel *)argument;

    channel->setup_result = L_SetupChannel(channel, &connection, &handle);
    L_PrintUserId(channel);
    L_WaitForStart();

    if (channel->setup_result == 0)
    {
        struct timespec stagger = {0};
        stagger.tv_nsec = (long)(channel->channel_id - 1) * 2000000L;
        (void)nanosleep(&stagger, NULL);
        channel->first_call_ns = L_MonotonicNs();
        for (iteration = 1; iteration <= channel->iterations; iteration++)
        {
            business_result = kHsedHsedResult_DEVICE_ERROR;
            signature_size = sizeof(signature);
            r_size = 0;
            s_size = 0;
            begin_ns = L_MonotonicNs();
            transport_result = L_CommandSign(
                connection, handle, channel->message, channel->message_size,
                &business_result, signature, &signature_size,
                &r_size, &s_size);
            end_ns = L_MonotonicNs();
            elapsed_ns = end_ns - begin_ns;

            L_PrintCallResult(channel->channel_id, iteration,
                              transport_result, business_result, elapsed_ns);
            if ((transport_result == SC_rSuccess) &&
                (business_result == kHsedHsedResult_OK) &&
                (signature_size > 0) &&
                ((r_size + s_size) == (unsigned int)signature_size))
            {
                channel->success_count++;
                if ((channel->sample_ready == False) &&
                    (signature_size == HSED_STRESS_SM2_SIGNATURE_LEN))
                {
                    memcpy(channel->sample_signature, signature,
                           HSED_STRESS_SM2_SIGNATURE_LEN);
                    channel->sample_signature_size = signature_size;
                    channel->sample_ready = True;
                }
                channel->total_latency_ns += elapsed_ns;
                if ((channel->min_latency_ns == 0) ||
                    (elapsed_ns < channel->min_latency_ns))
                {
                    channel->min_latency_ns = elapsed_ns;
                }
                if (elapsed_ns > channel->max_latency_ns)
                {
                    channel->max_latency_ns = elapsed_ns;
                }
            }
            else
            {
                channel->failure_count++;
            }
        }
        channel->last_call_ns = L_MonotonicNs();
    }

    L_CleanupChannel(connection, handle);
    return NULL;
}

static void L_InitChannel(THsedStressChannel *channel,
                          int channel_id,
                          int iterations,
                          int message_size,
                          int public_key_id,
                          int private_key_id)
{
    int index;

    memset(channel, 0, sizeof(*channel));
    channel->channel_id = channel_id;
    channel->iterations = iterations;
    channel->message_size = message_size;
    channel->public_key_id = public_key_id;
    channel->private_key_id = private_key_id;
    channel->setup_result = -1;
    for (index = 0; index < message_size; index++)
    {
        channel->message[index] = (unsigned char)((index + 1) & 0xFF);
    }
}

static int L_CreateVerifySample(THsedStressChannel *channel,
                                int message_size,
                                int public_key_id,
                                int private_key_id)
{
    int result = -1;
    int transport_result = SC_rFailure;
    int business_result = kHsedHsedResult_DEVICE_ERROR;
    int signature_size = HSED_STRESS_SM2_SIGNATURE_LEN;
    unsigned int r_size = 0;
    unsigned int s_size = 0;
    TInt64 handle = 0;
    void *connection = NULL;
    unsigned char signature[HSED_STRESS_SM2_SIGNATURE_LEN] = {0};

    L_InitChannel(channel, 0, 1, message_size,
                  public_key_id, private_key_id);
    channel->capture_sample = True;
    memcpy(channel->user_id, g_verify_user_id_,
           sizeof(g_verify_user_id_));
    if (L_SetupChannel(channel, &connection, &handle) == 0)
    {
        transport_result = L_CommandSign(
            connection, handle, channel->message, channel->message_size,
            &business_result, signature, &signature_size,
            &r_size, &s_size);
        if ((transport_result == SC_rSuccess) &&
            (business_result == kHsedHsedResult_OK) &&
            (signature_size == HSED_STRESS_SM2_SIGNATURE_LEN) &&
            (r_size == 32U) && (s_size == 32U))
        {
            memcpy(channel->sample_signature, signature,
                   HSED_STRESS_SM2_SIGNATURE_LEN);
            channel->sample_signature_size = signature_size;
            channel->sample_ready = True;
            result = 0;
        }
    }
    L_CleanupChannel(connection, handle);
    return result;
}

static void L_PrintHex(const char *name,
                       const unsigned char *data,
                       int data_size)
{
    int index;

    (void)printf("VERIFY_SAMPLE,%s=", name);
    for (index = 0; index < data_size; index++)
    {
        (void)printf("%02X", data[index]);
    }
    (void)printf("\n");
}

static void L_PrintVerifySample(const THsedStressChannel *channel)
{
    if (channel->sample_ready == True)
    {
        L_PrintHex("user_id", channel->user_id,
                   HSED_STRESS_USER_ID_LEN);
        L_PrintHex("message", channel->message,
                   channel->message_size);
        L_PrintHex("public_x", channel->sample_public_key, 32);
        L_PrintHex("public_y", channel->sample_public_key + 32, 32);
        L_PrintHex("signature", channel->sample_signature,
                   channel->sample_signature_size);
    }
    else
    {
        (void)printf("VERIFY_SAMPLE,unavailable=1\n");
    }
}

static int L_ValidateChannelIsolation(const THsedStressChannel *channels,
                                      int channel_count)
{
    int left;
    int right;

    for (left = 0; left < channel_count; left++)
    {
        if (channels[left].sample_ready == False)
        {
            return -1;
        }
        for (right = left + 1; right < channel_count; right++)
        {
            if (memcmp(channels[left].user_id, channels[right].user_id,
                       HSED_STRESS_USER_ID_LEN) == 0 ||
                channels[left].message_size != channels[right].message_size ||
                memcmp(channels[left].message, channels[right].message,
                       (size_t)channels[left].message_size) != 0 ||
                memcmp(channels[left].sample_signature,
                       channels[right].sample_signature,
                       HSED_STRESS_SM2_SIGNATURE_LEN) == 0)
            {
                return -1;
            }
        }
    }
    (void)printf("CHECK,unique_user_ids=1,same_message=1,"
                 "different_signatures=1\n");
    return 0;
}

static void L_PrintSummary(const THsedStressChannel *channels,
                           int channel_count)
{
    int index;
    int success_count = 0;
    int failure_count = 0;
    uint64_t first_call_ns = 0;
    uint64_t last_call_ns = 0;
    uint64_t total_latency_ns = 0;

    for (index = 0; index < channel_count; index++)
    {
        double average_us = 0.0;
        double channel_rate = 0.0;

        if (channels[index].success_count > 0)
        {
            average_us =
                ((double)channels[index].total_latency_ns / 1000.0) /
                (double)channels[index].success_count;
        }
        if (channels[index].last_call_ns >
            channels[index].first_call_ns)
        {
            channel_rate =
                ((double)channels[index].success_count * 1000000000.0) /
                (double)(channels[index].last_call_ns -
                         channels[index].first_call_ns);
        }
        (void)printf("SUMMARY,channel=%d,success=%d,failure=%d,"
                     "min_us=%.3f,avg_us=%.3f,max_us=%.3f,"
                     "signatures_per_second=%.2f\n",
                     channels[index].channel_id,
                     channels[index].success_count,
                     channels[index].failure_count,
                     (double)channels[index].min_latency_ns / 1000.0,
                     average_us,
                     (double)channels[index].max_latency_ns / 1000.0,
                     channel_rate);

        success_count += channels[index].success_count;
        failure_count += channels[index].failure_count;
        total_latency_ns += channels[index].total_latency_ns;
        if ((channels[index].first_call_ns > 0) &&
            ((first_call_ns == 0) ||
             (channels[index].first_call_ns < first_call_ns)))
        {
            first_call_ns = channels[index].first_call_ns;
        }
        if (channels[index].last_call_ns > last_call_ns)
        {
            last_call_ns = channels[index].last_call_ns;
        }
    }

    if ((success_count > 0) && (last_call_ns > first_call_ns))
    {
        (void)printf("TOTAL,channels=%d,success=%d,failure=%d,"
                     "avg_call_us=%.3f,signatures_per_second=%.2f\n",
                     channel_count, success_count, failure_count,
                     ((double)total_latency_ns / 1000.0) /
                         (double)success_count,
                     ((double)success_count * 1000000000.0) /
                         (double)(last_call_ns - first_call_ns));
    }
    else
    {
        (void)printf("TOTAL,channels=%d,success=%d,failure=%d\n",
                     channel_count, success_count, failure_count);
    }
}

static void L_PrintUsage(const char *program)
{
    (void)fprintf(stderr,
                  "Usage: %s [--channels 1-%d] [--iterations N] "
                  "[--message-size 1-%d]\n",
                  program, HSED_STRESS_MAX_CHANNELS,
                  HSED_STRESS_MAX_MESSAGE);
}

static Bool L_UserIdExists(const THsedStressChannel *channels,
                           int channel_count,
                           const unsigned char *user_id)
{
    int index;

    for (index = 0; index < channel_count; index++)
    {
        if (memcmp(channels[index].user_id, user_id,
                   HSED_STRESS_USER_ID_LEN) == 0)
        {
            return True;
        }
    }
    return False;
}

int main(int argc, char *argv[])
{
    int result = 1;
    int channel_count = HSED_STRESS_DEFAULT_CHANNELS;
    int iterations = HSED_STRESS_DEFAULT_ITERATIONS;
    int message_size = HSED_STRESS_DEFAULT_MESSAGE;
    int public_key_id = HSED_STRESS_DEFAULT_PUBLIC_KEY_ID;
    int private_key_id = HSED_STRESS_DEFAULT_PRIVATE_KEY_ID;
    int argument_index = 1;
    int index;
    int random_attempt;
    int created_threads = 0;
    int isolation_result = -1;
    Bool arguments_valid = True;
    pthread_t threads[HSED_STRESS_MAX_CHANNELS] = {0};
    THsedStressChannel channels[HSED_STRESS_MAX_CHANNELS] = {{0}};
    THsedStressChannel verify_sample = {0};

    while ((argument_index < argc) && (arguments_valid == True))
    {
        if ((argument_index + 1) >= argc)
        {
            arguments_valid = False;
        }
        else if (strcmp(argv[argument_index], "--channels") == 0)
        {
            arguments_valid =
                (L_ParsePositive(argv[argument_index + 1],
                                 HSED_STRESS_MAX_CHANNELS,
                                 &channel_count) == 0)
                    ? True : False;
        }
        else if (strcmp(argv[argument_index], "--iterations") == 0)
        {
            arguments_valid =
                (L_ParsePositive(argv[argument_index + 1], 1000000,
                                 &iterations) == 0)
                    ? True : False;
        }
        else if (strcmp(argv[argument_index], "--message-size") == 0)
        {
            arguments_valid =
                (L_ParsePositive(argv[argument_index + 1],
                                 HSED_STRESS_MAX_MESSAGE,
                                 &message_size) == 0)
                    ? True : False;
        }
        else
        {
            arguments_valid = False;
        }
        argument_index += 2;
    }

    if (arguments_valid == False)
    {
        L_PrintUsage(argv[0]);
    }
    else
    {
        SCInit();
        for (index = 0; index < channel_count; index++)
        {
            L_InitChannel(&channels[index], index + 1,
                          iterations, message_size,
                          public_key_id, private_key_id);
            for (random_attempt = 0; random_attempt < 16; random_attempt++)
            {
                if (L_LoadRandom(channels[index].user_id,
                                 sizeof(channels[index].user_id)) != 0)
                {
                    random_attempt = 16;
                    break;
                }
                if (L_UserIdExists(channels, index,
                                   channels[index].user_id) == False)
                {
                    break;
                }
            }
            if ((random_attempt >= 16) ||
                (L_UserIdExists(channels, index,
                                channels[index].user_id) == True))
            {
                (void)fprintf(stderr,
                              "[CHANNEL %d] unique random USER_ID failed\n",
                              channels[index].channel_id);
                break;
            }
            if (pthread_create(&threads[index], NULL, L_ChannelThread,
                               &channels[index]) != 0)
            {
                (void)fprintf(stderr,
                              "[CHANNEL %d] pthread_create failed\n",
                              channels[index].channel_id);
                break;
            }
            created_threads++;
        }

        (void)pthread_mutex_lock(&g_start_mutex_);
        while (g_ready_count_ < created_threads)
        {
            (void)pthread_cond_wait(&g_start_cond_, &g_start_mutex_);
        }
        g_start_ = True;
        (void)pthread_cond_broadcast(&g_start_cond_);
        (void)pthread_mutex_unlock(&g_start_mutex_);

        for (index = 0; index < created_threads; index++)
        {
            (void)pthread_join(threads[index], NULL);
        }
        if (L_CreateVerifySample(&verify_sample, message_size,
                                 public_key_id, private_key_id) != 0)
        {
            (void)fprintf(stderr,
                          "[VERIFY SAMPLE] fixed USER_ID signing failed\n");
        }
        SCFini();

        L_PrintSummary(channels, created_threads);
        isolation_result =
            L_ValidateChannelIsolation(channels, created_threads);
        if (isolation_result != 0)
        {
            (void)fprintf(stderr,
                          "CHECK,channel_isolation_failed=1\n");
        }
        L_PrintVerifySample(&verify_sample);
        if (created_threads == channel_count)
        {
            result = 0;
            for (index = 0; index < channel_count; index++)
            {
                if ((channels[index].setup_result != 0) ||
                    (channels[index].failure_count != 0) ||
                    (channels[index].success_count != iterations))
                {
                    result = 1;
                }
            }
            if (verify_sample.sample_ready == False)
            {
                result = 1;
            }
            if (isolation_result != 0)
            {
                result = 1;
            }
        }
    }
    return result;
}
