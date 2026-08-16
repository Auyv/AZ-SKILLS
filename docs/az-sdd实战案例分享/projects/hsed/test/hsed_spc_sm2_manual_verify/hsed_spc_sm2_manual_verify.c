/**
 * @file        hsed_spc_sm2_manual_verify.c
 * @brief       HSED SPC three-object SM2 manual verification test
 * @author      HSED
 * @date        2026/07/29
 * @version     1.0
 */

#include "generated/hsed_client.h"

#include "SpanCall.h"

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <stdio.h>
#include <string.h>

#define HSED_MANUAL_GROUP_COUNT          3
#define HSED_MANUAL_USER_ID_LEN          16
#define HSED_MANUAL_MESSAGE_LEN          1024
#define HSED_MANUAL_PUBLIC_KEY_LEN       64
#define HSED_MANUAL_SIGNATURE_CAPACITY   128
#define HSED_MANUAL_SM2_SIGNATURE_LEN    64
#define HSED_MANUAL_SM2_COMPONENT_LEN    32
#define HSED_MANUAL_SM2_PUBLIC_KEY_ID    11
#define HSED_MANUAL_SM2_PRIVATE_KEY_ID   12
#define HSED_MANUAL_RANDOM_RETRY_COUNT   32

typedef struct THsedManualSampleTag THsedManualSample;
struct THsedManualSampleTag
{
    TInt64         handle;
    Bool           object_created;
    unsigned char  user_id[HSED_MANUAL_USER_ID_LEN];
    unsigned char  public_key[HSED_MANUAL_PUBLIC_KEY_LEN];
    unsigned char  message[HSED_MANUAL_MESSAGE_LEN];
    unsigned char  signature[HSED_MANUAL_SIGNATURE_CAPACITY];
    int            public_key_size;
    int            signature_size;
    unsigned int   r_size;
    unsigned int   s_size;
};

static int L_CheckTransport(const char *operation, int transport_result)
{
    int result = 0;

    if (transport_result != SC_rSuccess)
    {
        (void)fprintf(stderr, "[FAIL] %s transport=%d\n",
                      operation, transport_result);
        result = -1;
    }
    return result;
}

static int L_CheckCall(const char *operation,
                       int group_index,
                       int transport_result,
                       int business_result)
{
    int result = 0;

    if (transport_result != SC_rSuccess)
    {
        (void)fprintf(stderr,
                      "[FAIL] group=%d %s transport=%d\n",
                      group_index + 1, operation, transport_result);
        result = -1;
    }
    else if (business_result != kHsedHsedResult_OK)
    {
        (void)fprintf(stderr,
                      "[FAIL] group=%d %s business=%d\n",
                      group_index + 1, operation, business_result);
        result = -1;
    }
    return result;
}

static int L_LoadRandom(unsigned char *buffer, int size)
{
    int result = -1;
    FILE *random_file = NULL;

    if ((buffer != NULL) && (size > 0))
    {
        random_file = fopen("/dev/urandom", "rb");
        if (random_file != NULL)
        {
            if (fread(buffer, 1, (size_t)size, random_file) ==
                (size_t)size)
            {
                result = 0;
            }
            (void)fclose(random_file);
        }
    }
    return result;
}

static Bool L_IsUserIdUnique(const THsedManualSample *samples,
                             int current_index)
{
    Bool result = True;
    int index = 0;

    while ((index < current_index) && (result == True))
    {
        if (memcmp(samples[index].user_id,
                   samples[current_index].user_id,
                   HSED_MANUAL_USER_ID_LEN) == 0)
        {
            result = False;
        }
        index++;
    }
    return result;
}

static int L_CreateObjects(THsedManualSample *samples)
{
    int result = 0;
    int index = 0;

    while ((index < HSED_MANUAL_GROUP_COUNT) && (result == 0))
    {
        int business_result = kHsedHsedResult_DEVICE_ERROR;
        int transport_result = HcHsed_HseCreate(
            &business_result, &samples[index].handle);

        result = L_CheckCall("HseCreate", index,
                             transport_result, business_result);
        if (result == 0)
        {
            samples[index].object_created = True;
        }
        index++;
    }
    return result;
}

static int L_SetDistinctUserIds(THsedManualSample *samples)
{
    int result = 0;
    int index = 0;

    while ((index < HSED_MANUAL_GROUP_COUNT) && (result == 0))
    {
        Bool unique = False;
        int attempt = 0;

        while ((attempt < HSED_MANUAL_RANDOM_RETRY_COUNT) &&
               (unique == False) && (result == 0))
        {
            if (L_LoadRandom(samples[index].user_id,
                             HSED_MANUAL_USER_ID_LEN) != 0)
            {
                (void)fprintf(stderr,
                              "[FAIL] group=%d random USER_ID failed\n",
                              index + 1);
                result = -1;
            }
            else
            {
                unique = L_IsUserIdUnique(samples, index);
            }
            attempt++;
        }

        if ((result == 0) && (unique == False))
        {
            (void)fprintf(stderr,
                          "[FAIL] group=%d unique USER_ID failed\n",
                          index + 1);
            result = -1;
        }
        if (result == 0)
        {
            int business_result = kHsedHsedResult_DEVICE_ERROR;
            int transport_result = HcHsed_SetPropertyBytes(
                samples[index].handle,
                kHsedHsedPropertyId_USER_ID,
                samples[index].user_id,
                HSED_MANUAL_USER_ID_LEN,
                &business_result);

            result = L_CheckCall("SetPropertyBytes(USER_ID)", index,
                                 transport_result, business_result);
        }
        index++;
    }
    return result;
}

static int L_CheckFixedSm2KeyIds(TInt64 handle, int group_index)
{
    int result = 0;
    int business_result = kHsedHsedResult_DEVICE_ERROR;
    int public_key_id = 0;
    int private_key_id = 0;
    int transport_result = HcHsed_GetPropertyInt(
        handle,
        kHsedHsedPropertyId_SM2_PUBLIC_KEY_ID,
        &business_result,
        &public_key_id);

    result = L_CheckCall("GetPropertyInt(SM2_PUBLIC_KEY_ID)",
                         group_index,
                         transport_result,
                         business_result);
    if (result == 0)
    {
        business_result = kHsedHsedResult_DEVICE_ERROR;
        transport_result = HcHsed_GetPropertyInt(
            handle,
            kHsedHsedPropertyId_SM2_PRIVATE_KEY_ID,
            &business_result,
            &private_key_id);
        result = L_CheckCall("GetPropertyInt(SM2_PRIVATE_KEY_ID)",
                             group_index,
                             transport_result,
                             business_result);
    }
    if ((result == 0) &&
        ((public_key_id != HSED_MANUAL_SM2_PUBLIC_KEY_ID) ||
         (private_key_id != HSED_MANUAL_SM2_PRIVATE_KEY_ID)))
    {
        (void)fprintf(
            stderr,
            "[FAIL] group=%d fixed SM2 slots public=%d private=%d\n",
            group_index + 1, public_key_id, private_key_id);
        result = -1;
    }
    return result;
}

static int L_ExportPublicKeys(THsedManualSample *samples)
{
    int result = 0;
    int index = 0;

    while ((index < HSED_MANUAL_GROUP_COUNT) && (result == 0))
    {
        result = L_CheckFixedSm2KeyIds(samples[index].handle, index);
        if (result == 0)
        {
            int business_result = kHsedHsedResult_DEVICE_ERROR;
            int transport_result;

            samples[index].public_key_size =
                HSED_MANUAL_PUBLIC_KEY_LEN;
            transport_result = HcHsed_ExportPublicKey(
                samples[index].handle,
                kHsedHsedAlgorithm_SM2,
                &business_result,
                samples[index].public_key,
                &samples[index].public_key_size);
            result = L_CheckCall("ExportPublicKey", index,
                                 transport_result, business_result);
            if ((result == 0) &&
                (samples[index].public_key_size !=
                 HSED_MANUAL_PUBLIC_KEY_LEN))
            {
                (void)fprintf(
                    stderr,
                    "[FAIL] group=%d SM2 public key size=%d expected=%d\n",
                    index + 1,
                    samples[index].public_key_size,
                    HSED_MANUAL_PUBLIC_KEY_LEN);
                result = -1;
            }
        }
        index++;
    }
    return result;
}

static int L_ComparePublicKeys(const THsedManualSample *samples)
{
    int result = 0;
    int index = 1;

    while ((index < HSED_MANUAL_GROUP_COUNT) && (result == 0))
    {
        if ((samples[index].public_key_size !=
             samples[0].public_key_size) ||
            (memcmp(samples[index].public_key,
                    samples[0].public_key,
                    HSED_MANUAL_PUBLIC_KEY_LEN) != 0))
        {
            (void)fprintf(stderr,
                          "[FAIL] group=%d public key differs from group=1\n",
                          index + 1);
            result = -1;
        }
        index++;
    }
    if (result == 0)
    {
        (void)printf("[PASS] all 3 objects exported the same SM2 public key\n");
    }
    return result;
}

static int L_SignRandomMessages(THsedManualSample *samples)
{
    int result = 0;
    int index = 0;

    while ((index < HSED_MANUAL_GROUP_COUNT) && (result == 0))
    {
        if (L_LoadRandom(samples[index].message,
                         HSED_MANUAL_MESSAGE_LEN) != 0)
        {
            (void)fprintf(stderr,
                          "[FAIL] group=%d random message failed\n",
                          index + 1);
            result = -1;
        }
        else
        {
            int business_result = kHsedHsedResult_DEVICE_ERROR;
            int transport_result;

            samples[index].signature_size =
                HSED_MANUAL_SIGNATURE_CAPACITY;
            samples[index].r_size = 0U;
            samples[index].s_size = 0U;
            transport_result = HcHsed_Sign(
                samples[index].handle,
                kHsedHsedAlgorithm_SM2,
                samples[index].message,
                HSED_MANUAL_MESSAGE_LEN,
                &business_result,
                samples[index].signature,
                &samples[index].signature_size,
                &samples[index].r_size,
                &samples[index].s_size);
            result = L_CheckCall("Sign", index,
                                 transport_result, business_result);
            if ((result == 0) &&
                ((samples[index].signature_size !=
                  HSED_MANUAL_SM2_SIGNATURE_LEN) ||
                 (samples[index].r_size !=
                  HSED_MANUAL_SM2_COMPONENT_LEN) ||
                 (samples[index].s_size !=
                  HSED_MANUAL_SM2_COMPONENT_LEN)))
            {
                (void)fprintf(
                    stderr,
                    "[FAIL] group=%d signature=%d r=%u s=%u\n",
                    index + 1,
                    samples[index].signature_size,
                    samples[index].r_size,
                    samples[index].s_size);
                result = -1;
            }
        }
        index++;
    }
    return result;
}

static void L_PrintHex(const char *label,
                       const unsigned char *data,
                       int size)
{
    int index = 0;

    (void)printf("%s=", label);
    while (index < size)
    {
        (void)printf("%02X", data[index]);
        index++;
    }
    (void)printf("\n");
}

static void L_PrintSamples(const THsedManualSample *samples)
{
    int index = 0;

    while (index < HSED_MANUAL_GROUP_COUNT)
    {
        (void)printf("\n========== GROUP %d ==========\n", index + 1);
        L_PrintHex("USER_ID",
                   samples[index].user_id,
                   HSED_MANUAL_USER_ID_LEN);
        L_PrintHex("PUBLIC_KEY",
                   samples[index].public_key,
                   samples[index].public_key_size);
        L_PrintHex("PUBLIC_KEY_X",
                   samples[index].public_key,
                   HSED_MANUAL_SM2_COMPONENT_LEN);
        L_PrintHex("PUBLIC_KEY_Y",
                   &samples[index].public_key[
                       HSED_MANUAL_SM2_COMPONENT_LEN],
                   HSED_MANUAL_SM2_COMPONENT_LEN);
        L_PrintHex("MESSAGE",
                   samples[index].message,
                   HSED_MANUAL_MESSAGE_LEN);
        L_PrintHex("SIGNATURE",
                   samples[index].signature,
                   samples[index].signature_size);
        L_PrintHex("SIGNATURE_R",
                   samples[index].signature,
                   (int)samples[index].r_size);
        L_PrintHex("SIGNATURE_S",
                   &samples[index].signature[samples[index].r_size],
                   (int)samples[index].s_size);
        (void)printf("MESSAGE_SIZE=%d\n", HSED_MANUAL_MESSAGE_LEN);
        (void)printf("SIGNATURE_SIZE=%d\n",
                     samples[index].signature_size);
        index++;
    }
}

static int L_WaitManualResults(void)
{
    int result = 0;
    int index = 0;
    Bool input_available = True;

    while ((index < HSED_MANUAL_GROUP_COUNT) &&
           (input_available == True))
    {
        char answer[32] = {0};
        int answer_index = 0;
        char selection = '\0';

        (void)printf(
            "\nMANUAL_VERIFY_GROUP_%d [y=pass/n=fail]: ",
            index + 1);
        (void)fflush(stdout);
        if (fgets(answer, (int)sizeof(answer), stdin) == NULL)
        {
            (void)fprintf(stderr,
                          "\n[FAIL] group=%d manual result input ended\n",
                          index + 1);
            result = -1;
            input_available = False;
        }
        else
        {
            while ((answer[answer_index] == ' ') ||
                   (answer[answer_index] == '\t'))
            {
                answer_index++;
            }
            selection = answer[answer_index];
            if ((selection == 'y') || (selection == 'Y'))
            {
                (void)printf("[PASS] group=%d manual verification\n",
                             index + 1);
            }
            else if ((selection == 'n') || (selection == 'N'))
            {
                (void)fprintf(stderr,
                              "[FAIL] group=%d manual verification\n",
                              index + 1);
                result = -1;
            }
            else
            {
                (void)fprintf(stderr,
                              "[FAIL] group=%d invalid input\n",
                              index + 1);
                result = -1;
            }
        }
        index++;
    }
    return result;
}

static EVP_PKEY *L_CreateSm2PublicKey(const unsigned char *public_key)
{
    EVP_PKEY *result = NULL;
    EVP_PKEY_CTX *context = NULL;
    unsigned char encoded_key[HSED_MANUAL_PUBLIC_KEY_LEN + 1] = {0};
    char group_name[] = "SM2";
    OSSL_PARAM params[3];

    if (public_key != NULL)
    {
        encoded_key[0] = 0x04U;
        memcpy(&encoded_key[1], public_key, HSED_MANUAL_PUBLIC_KEY_LEN);
        params[0] = OSSL_PARAM_construct_utf8_string(
            OSSL_PKEY_PARAM_GROUP_NAME, group_name, 0);
        params[1] = OSSL_PARAM_construct_octet_string(
            OSSL_PKEY_PARAM_PUB_KEY, encoded_key, sizeof(encoded_key));
        params[2] = OSSL_PARAM_construct_end();
        context = EVP_PKEY_CTX_new_from_name(NULL, "SM2", NULL);
        if ((context != NULL) &&
            (EVP_PKEY_fromdata_init(context) > 0) &&
            (EVP_PKEY_fromdata(context, &result,
                               EVP_PKEY_PUBLIC_KEY, params) <= 0))
        {
            result = NULL;
        }
    }
    EVP_PKEY_CTX_free(context);

    return result;
}

static int L_EncodeSm2Signature(const THsedManualSample *sample,
                                unsigned char *encoded,
                                int *encoded_size)
{
    int result = -1;
    ECDSA_SIG *signature = NULL;
    BIGNUM *r = NULL;
    BIGNUM *s = NULL;
    unsigned char *output = encoded;
    int required_size;

    if ((sample != NULL) && (encoded != NULL) && (encoded_size != NULL) &&
        (sample->r_size == HSED_MANUAL_SM2_COMPONENT_LEN) &&
        (sample->s_size == HSED_MANUAL_SM2_COMPONENT_LEN))
    {
        r = BN_bin2bn(sample->signature, (int)sample->r_size, NULL);
        s = BN_bin2bn(&sample->signature[sample->r_size],
                      (int)sample->s_size, NULL);
        signature = ECDSA_SIG_new();
        if ((r != NULL) && (s != NULL) && (signature != NULL) &&
            (ECDSA_SIG_set0(signature, r, s) == 1))
        {
            r = NULL;
            s = NULL;
            required_size = i2d_ECDSA_SIG(signature, NULL);
            if ((required_size > 0) && (required_size <= *encoded_size) &&
                (i2d_ECDSA_SIG(signature, &output) == required_size))
            {
                *encoded_size = required_size;
                result = 0;
            }
        }
    }
    BN_clear_free(r);
    BN_clear_free(s);
    ECDSA_SIG_free(signature);

    return result;
}

static int L_VerifySample(const THsedManualSample *sample)
{
    int result = -1;
    EVP_PKEY *public_key = NULL;
    EVP_PKEY_CTX *verify_context = NULL;
    EVP_MD_CTX *digest_context = NULL;
    unsigned char encoded_signature[80] = {0};
    int encoded_signature_size = (int)sizeof(encoded_signature);

    if (sample != NULL)
    {
        public_key = L_CreateSm2PublicKey(sample->public_key);
        digest_context = EVP_MD_CTX_new();
        if ((public_key != NULL) && (digest_context != NULL) &&
            (L_EncodeSm2Signature(sample, encoded_signature,
                                  &encoded_signature_size) == 0) &&
            (EVP_DigestVerifyInit(digest_context, &verify_context,
                                  EVP_sm3(), NULL, public_key) > 0) &&
            (EVP_PKEY_CTX_set1_id(verify_context, sample->user_id,
                                  HSED_MANUAL_USER_ID_LEN) > 0) &&
            (EVP_DigestVerifyUpdate(digest_context, sample->message,
                                    HSED_MANUAL_MESSAGE_LEN) > 0) &&
            (EVP_DigestVerifyFinal(digest_context, encoded_signature,
                                   (size_t)encoded_signature_size) == 1))
        {
            result = 0;
        }
    }
    EVP_MD_CTX_free(digest_context);
    EVP_PKEY_free(public_key);

    return result;
}

static int L_VerifySamplesAutomatically(const THsedManualSample *samples)
{
    int result = 0;
    int index = 0;

    while ((index < HSED_MANUAL_GROUP_COUNT) && (result == 0))
    {
        if (L_VerifySample(&samples[index]) == 0)
        {
            (void)printf("[PASS] group=%d OpenSSL SM2 ZA verification\n",
                         index + 1);
        }
        else
        {
            (void)fprintf(stderr,
                          "[FAIL] group=%d OpenSSL SM2 ZA verification\n",
                          index + 1);
            result = -1;
        }
        index++;
    }

    return result;
}

static void L_DestroyObjects(THsedManualSample *samples)
{
    int index = HSED_MANUAL_GROUP_COUNT - 1;

    while (index >= 0)
    {
        if (samples[index].object_created == True)
        {
            int business_result = kHsedHsedResult_DEVICE_ERROR;
            int transport_result = HcHsed_HseDestroy(
                samples[index].handle, &business_result);

            if ((transport_result != SC_rSuccess) ||
                (business_result != kHsedHsedResult_OK))
            {
                (void)fprintf(
                    stderr,
                    "[WARN] group=%d HseDestroy transport=%d business=%d\n",
                    index + 1, transport_result, business_result);
            }
            samples[index].object_created = False;
        }
        index--;
    }
}

int main(int argc, char *argv[])
{
    int result = 1;
    Bool client_initialized = False;
    Bool connection_opened = False;
    Bool manual_mode = False;
    THsedManualSample samples[HSED_MANUAL_GROUP_COUNT] = {{0}};

    if ((argc > 1) && (strcmp(argv[1], "--manual") == 0))
    {
        manual_mode = True;
    }

    SCInit();
    if (L_CheckTransport("client init", HcHsed_Init()) == 0)
    {
        client_initialized = True;
        if (L_CheckTransport("connection open", HcHsed_Open()) == 0)
        {
            connection_opened = True;
            if ((L_CreateObjects(samples) == 0) &&
                (L_SetDistinctUserIds(samples) == 0) &&
                (L_ExportPublicKeys(samples) == 0) &&
                (L_ComparePublicKeys(samples) == 0) &&
                (L_SignRandomMessages(samples) == 0))
            {
                L_PrintSamples(samples);
                (void)printf(
                    "\nUse USER_ID, PUBLIC_KEY_X/Y, MESSAGE and "
                    "SIGNATURE_R/S for SM2 verification with ZA.\n");
                if (((manual_mode == True) &&
                     (L_WaitManualResults() == 0)) ||
                    ((manual_mode == False) &&
                     (L_VerifySamplesAutomatically(samples) == 0)))
                {
                    result = 0;
                }
            }
        }
    }

    L_DestroyObjects(samples);
    if (connection_opened == True)
    {
        HcHsed_Close();
    }
    if (client_initialized == True)
    {
        HcHsed_Done();
    }
    SCFini();

    if (result == 0)
    {
        (void)printf("\n[PASS] all SM2 verifications passed\n");
    }
    else
    {
        (void)printf("\n[FAIL] SM2 manual verification test failed\n");
    }
    return result;
}
