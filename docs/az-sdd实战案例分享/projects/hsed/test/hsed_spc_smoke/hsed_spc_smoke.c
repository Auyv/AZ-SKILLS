/**
 * @file        hsed_spc_smoke.c
 * @brief       HSED SPC target smoke test client
 * @author      HSED
 * @date        2026/07/24
 * @version     1.0
 */

#include "generated/hsed_client.h"

#include "SpanCall.h"

#include <stdio.h>
#include <string.h>

#define HSED_SMOKE_USER_ID_LEN 32
#define HSED_SMOKE_AES_BLOCK_LEN 16

static int L_CheckTransport(const char *operation, int transport_result)
{
    if (transport_result != SC_rSuccess)
    {
        (void)fprintf(stderr, "[FAIL] %s transport=%d\n",
                      operation, transport_result);
        return -1;
    }
    return 0;
}

static int L_CheckBusiness(const char *operation, int business_result)
{
    if (business_result != kHsedHsedResult_OK)
    {
        (void)fprintf(stderr, "[FAIL] %s business=%d\n",
                      operation, business_result);
        return -1;
    }
    return 0;
}

static int L_RunLifecycleAndPropertyTest(TInt64 handle)
{
    int result = -1;
    int business_result = kHsedHsedResult_DEVICE_ERROR;
    int value = 0;
    int user_id_size = HSED_SMOKE_USER_ID_LEN;
    unsigned char user_id[HSED_SMOKE_USER_ID_LEN] =
    {
        'H', 'S', 'E', 'D', '-', 'S', 'P', 'C',
        '-', 'S', 'M', 'O', 'K', 'E', '-', '0',
        '0', '0', '0', '0', '0', '0', '0', '0',
        '0', '0', '0', '0', '0', '0', '0', '1'
    };
    unsigned char returned_user_id[HSED_SMOKE_USER_ID_LEN] = {0};

    if ((L_CheckTransport(
             "SetPropertyInt",
             HcHsed_SetPropertyInt(
                 handle,
                 kHsedHsedPropertyId_AES128_CIPHER_MODE,
                 kHsedHsedCipherMode_ECB,
                 &business_result)) == 0) &&
        (L_CheckBusiness("SetPropertyInt", business_result) == 0) &&
        (L_CheckTransport(
             "GetPropertyInt",
             HcHsed_GetPropertyInt(
                 handle,
                 kHsedHsedPropertyId_AES128_CIPHER_MODE,
                 &business_result,
                 &value)) == 0) &&
        (L_CheckBusiness("GetPropertyInt", business_result) == 0) &&
        (value == kHsedHsedCipherMode_ECB) &&
        (L_CheckTransport(
             "SetPropertyBytes",
             HcHsed_SetPropertyBytes(
                 handle,
                 kHsedHsedPropertyId_USER_ID,
                 user_id,
                 HSED_SMOKE_USER_ID_LEN,
                 &business_result)) == 0) &&
        (L_CheckBusiness("SetPropertyBytes", business_result) == 0) &&
        (L_CheckTransport(
             "GetPropertyBytes",
             HcHsed_GetPropertyBytes(
                 handle,
                 kHsedHsedPropertyId_USER_ID,
                 &business_result,
                 returned_user_id,
                 &user_id_size)) == 0) &&
        (L_CheckBusiness("GetPropertyBytes", business_result) == 0) &&
        (user_id_size == HSED_SMOKE_USER_ID_LEN) &&
        (memcmp(user_id, returned_user_id, HSED_SMOKE_USER_ID_LEN) == 0))
    {
        result = 0;
    }
    else if (value != kHsedHsedCipherMode_ECB)
    {
        (void)fprintf(stderr, "[FAIL] integer property mismatch: %d\n", value);
    }
    else if ((user_id_size != HSED_SMOKE_USER_ID_LEN) ||
             (memcmp(user_id, returned_user_id,
                     HSED_SMOKE_USER_ID_LEN) != 0))
    {
        (void)fprintf(stderr, "[FAIL] byte property mismatch: size=%d\n",
                      user_id_size);
    }
    return result;
}

static int L_RunAesRoundTripTest(TInt64 handle)
{
    int result = -1;
    int business_result = kHsedHsedResult_DEVICE_ERROR;
    int encrypted_size = HSED_SMOKE_AES_BLOCK_LEN;
    int decrypted_size = HSED_SMOKE_AES_BLOCK_LEN;
    unsigned char input[HSED_SMOKE_AES_BLOCK_LEN] =
    {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };
    unsigned char encrypted[HSED_SMOKE_AES_BLOCK_LEN] = {0};
    unsigned char decrypted[HSED_SMOKE_AES_BLOCK_LEN] = {0};

    if ((L_CheckTransport(
             "Encrypt",
             HcHsed_Encrypt(
                 handle,
                 kHsedHsedAlgorithm_AES128,
                 input,
                 HSED_SMOKE_AES_BLOCK_LEN,
                 &business_result,
                 encrypted,
                 &encrypted_size)) == 0) &&
        (L_CheckBusiness("Encrypt", business_result) == 0) &&
        (L_CheckTransport(
             "Decrypt",
             HcHsed_Decrypt(
                 handle,
                 kHsedHsedAlgorithm_AES128,
                 encrypted,
                 encrypted_size,
                 &business_result,
                 decrypted,
                 &decrypted_size)) == 0) &&
        (L_CheckBusiness("Decrypt", business_result) == 0) &&
        (decrypted_size == HSED_SMOKE_AES_BLOCK_LEN) &&
        (memcmp(input, decrypted, HSED_SMOKE_AES_BLOCK_LEN) == 0))
    {
        result = 0;
    }
    else if ((decrypted_size != HSED_SMOKE_AES_BLOCK_LEN) ||
             (memcmp(input, decrypted, HSED_SMOKE_AES_BLOCK_LEN) != 0))
    {
        (void)fprintf(stderr, "[FAIL] AES round trip mismatch: size=%d\n",
                      decrypted_size);
    }
    return result;
}

int main(int argc, char *argv[])
{
    int result = 1;
    int business_result = kHsedHsedResult_DEVICE_ERROR;
    Bool client_initialized = False;
    Bool connection_opened = False;
    Bool object_created = False;
    Bool run_aes_test = False;
    TInt64 handle = 0;

    if ((argc == 2) && (strcmp(argv[1], "--aes") == 0))
    {
        run_aes_test = True;
    }
    else if (argc != 1)
    {
        (void)fprintf(stderr, "Usage: %s [--aes]\n", argv[0]);
        return result;
    }

    SCInit();
    if (L_CheckTransport("client init", HcHsed_Init()) == 0)
    {
        client_initialized = True;
        if (L_CheckTransport("connection open", HcHsed_Open()) == 0)
        {
            connection_opened = True;
            if ((L_CheckTransport(
                     "HseCreate",
                     HcHsed_HseCreate(&business_result, &handle)) == 0) &&
                (L_CheckBusiness("HseCreate", business_result) == 0))
            {
                object_created = True;
                if ((L_RunLifecycleAndPropertyTest(handle) == 0) &&
                    ((run_aes_test == False) ||
                     (L_RunAesRoundTripTest(handle) == 0)))
                {
                    result = 0;
                }
            }
        }
    }

    if (object_created == True)
    {
        int destroy_result = kHsedHsedResult_DEVICE_ERROR;
        int transport_result = HcHsed_HseDestroy(handle, &destroy_result);
        if ((L_CheckTransport("HseDestroy", transport_result) != 0) ||
            (L_CheckBusiness("HseDestroy", destroy_result) != 0))
        {
            result = 1;
        }
    }
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
        (void)printf("[PASS] HSED SPC lifecycle/property smoke test");
        if (run_aes_test == True)
        {
            (void)printf(" and AES128 round trip");
        }
        (void)printf("\n");
    }
    return result;
}
