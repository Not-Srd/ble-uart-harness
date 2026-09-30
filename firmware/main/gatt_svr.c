/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "bleprph.h"
#include "services/ans/ble_svc_ans.h"

/*** Maximum number of characteristics with the notify flag ***/
#define MAX_NOTIFY 5
#define ECHO_MAX_LEN 20

//d5acca51-d28e-4f8c-97df-eece7a36fc23
static const ble_uuid128_t harness_svc_uuid = BLE_UUID128_INIT(0x23, 0xfc, 0x36, 0x7a, 0xce, 0xee, 0xdf, 0x97, 0x8c, 0x4f, 0x8e, 0xd2, 0x51, 0xca, 0xac, 0xd5); 

//d5acca52-d28e-4f8c-97df-eece7a36fc23
static const ble_uuid128_t echo_chr_uuid = BLE_UUID128_INIT(0x23, 0xfc, 0x36, 0x7a, 0xce, 0xee, 0xdf, 0x97, 0x8c, 0x4f, 0x8e, 0xd2, 0x52, 0xca, 0xac, 0xd5); 
static uint16_t echo_val_handle;

//d5acca53-d28e-4f8c-97df-eece7a36fc23
static const ble_uuid128_t stream_chr_uuid = BLE_UUID128_INIT(0x23, 0xfc, 0x36, 0x7a, 0xce, 0xee, 0xdf, 0x97, 0x8c, 0x4f, 0x8e, 0xd2, 0x53, 0xca, 0xac, 0xd5); 
static uint16_t stream_val_handle;

static int
gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt,
                void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service ***/
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &harness_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                /*** Laptop writesm ESP32 notifies the same bytes back ***/
                .uuid = &echo_chr_uuid.u,
                .access_cb = gatt_svc_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &echo_val_handle,

            }, {
                /*** ESP32 sends numbered notifications after START_STREAM over UART ***/
                .uuid = &stream_chr_uuid.u,
                .access_cb = gatt_svc_access,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &stream_val_handle,

            }, {
                0, /* No more characteristics in this service. */
            }
        },
    },

    {
        0, /* No more services. */
    },
};

static int
gatt_svr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
               void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

   /* Called by NimBLE when the laptop accesses a characteristic.
    * Only one case is possible with this table: a write to ECHO.
    * Valid writes (1 to ECHO_MAX_LEN bytes) are notified straight back
    * to the writer. Invalid writes are rejected with no notification. */

    
static int
gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* The only access we handle: a write to ECHO. */
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR &&
        attr_handle == echo_val_handle) {

        uint8_t buf[ECHO_MAX_LEN];
        uint16_t len;

        /* 1. Copy the written bytes out. Reject empty or oversize writes. */
        int rc = gatt_svr_write(ctxt->om, 1, ECHO_MAX_LEN, buf, &len);
        if (rc != 0) {
            MODLOG_DFLT(WARN, "ECHO write rejected; rc=%d\n", rc);
            return rc;                  /* laptop gets the ATT error, no notify */
        }

        /* 2. Wrap the same bytes in a fresh packet buffer. */
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, len);
        if (om == NULL) {
            MODLOG_DFLT(ERROR, "ECHO: no mbuf for notification\n");
            return 0;                   /* write itself succeeded */
        }

        /* 3. Notify them back to the device that wrote them. */
        rc = ble_gatts_notify_custom(conn_handle, echo_val_handle, om);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "ECHO: notify failed; rc=%d\n", rc);
        } else {
            MODLOG_DFLT(INFO, "ECHO: %d bytes echoed; conn_handle=%d\n",
                        len, conn_handle);
        }
        return 0;
    }

    /* Nothing else should ever reach here. */
    return BLE_ATT_ERR_UNLIKELY;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                    "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_init(void)
{
    int rc;

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    ble_svc_gap_init();
#endif /* CONFIG_BT_NIMBLE_GAP_SERVICE */
#if MYNEWT_VAL(BLE_GATTS)
    ble_svc_gatt_init();
#endif
#if CONFIG_BT_NIMBLE_ANS_SERVICE
    ble_svc_ans_init();
#endif

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
