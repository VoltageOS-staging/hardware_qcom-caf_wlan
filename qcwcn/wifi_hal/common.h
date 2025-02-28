/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *   * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hardware_legacy/wifi_hal.h>

#ifndef __WIFI_HAL_COMMON_H__
#define __WIFI_HAL_COMMON_H__

#ifndef LOG_TAG
#define LOG_TAG  "WifiHAL"
#endif

#include <stdint.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/filter.h>
#include <linux/errqueue.h>

#include <linux/pkt_sched.h>
#include <netlink/object-api.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>

#include "nl80211_copy.h"

#include <utils/Log.h>
#include "rb_wrapper.h"
#include "pkt_stats.h"
#include "wifihal_internal.h"
#include "qca-vendor_copy.h"

#define SOCKET_BUFFER_SIZE      (32768U)
#define RECV_BUF_SIZE           (4096)
#define DEFAULT_EVENT_CB_SIZE   (64)
#define NUM_RING_BUFS           5
#define MAX_NUM_RADAR_HISTORY   64
#define MAX_NUM_MLO_LINKS       15

#define WIFI_HAL_CTRL_IFACE     "/dev/socket/wifihal/wifihal_ctrlsock"

#ifdef CONFIG_MAC_PRIVACY_LOGGING
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[5]
#define MACSTR "%02x:%02x:%02x:**:**:%02x"
#else
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#define MAC_ADDR_ARRAY(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MAC_ADDR_STR "%02x:%02x:%02x:%02x:%02x:%02x"
#ifndef BIT
#define BIT(x) (1 << (x))
#endif

#define DEFAULT_NAN_IFACE    "wifi-aware0"

typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef void (*wifi_internal_event_handler) (wifi_handle handle, int events);

class WifiCommand;

typedef struct {
    int nl_cmd;
    uint32_t vendor_id;
    int vendor_subcmd;
    nl_recvmsg_msg_cb_t cb_func;
    void *cb_arg;
} cb_info;

typedef struct {
    wifi_request_id id;
    WifiCommand *cmd;
} cmd_info;

typedef struct {
    wifi_handle handle;                             // handle to wifi data
    char name[IFNAMSIZ+1];                          // interface name + trailing null
    int  id;                                        // id to use when talking to driver
} interface_info;

typedef struct {
    wifi_gscan_capabilities gscan_capa;
    wifi_roaming_capabilities roaming_capa;
    int max_mlo_association_link_count;
    int max_mlo_str_link_count;
} wifi_capa;

typedef struct {
    u8 *flags;
    size_t flags_len;
} features_info;

enum pkt_log_version {
    PKT_LOG_V0          = 0,     // UNSPECIFIED Target
    PKT_LOG_V1          = 1,     // ROME Base Target
    PKT_LOG_V2          = 2,     // HELIUM Base Target
    PKT_LOG_V3          = 3,     // LETHIUM Base target
};

struct gscan_event_handlers_s;
struct rssi_monitor_event_handler_s;
struct wpa_secure_nan;

struct ctrl_sock {
    int s;
    struct sockaddr_un local;
};

typedef struct hal_info_s {

    struct nl_sock *cmd_sock;                       // command socket object
    struct nl_sock *event_sock;                     // event socket object
    struct nl_sock *user_sock;                      // user socket object
    struct ctrl_sock wifihal_ctrl_sock;             // ctrl sock object
    struct list_head monitor_sockets;               // list of monitor sockets
    int nl80211_family_id;                          // family id for 80211 driver

    bool in_event_loop;                             // Indicates that event loop is active
    bool clean_up;                                  // Indication to clean up the socket

    wifi_internal_event_handler event_handler;      // default event handler
    wifi_cleaned_up_handler cleaned_up_handler;     // socket cleaned up handler

    cb_info *event_cb;                              // event callbacks
    int num_event_cb;                               // number of event callbacks
    int alloc_event_cb;                             // number of allocated callback objects
    pthread_mutex_t cb_lock;                        // mutex for the event_cb access

    interface_info **interfaces;                    // array of interfaces
    int num_interfaces;                             // number of interfaces

    feature_set supported_feature_set;
    /* driver supported features defined by enum qca_wlan_vendor_features that
       can be queried by vendor command QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES */
    features_info driver_supported_features;
    u32 supported_logger_feature_set;
    // add other details
    int user_sock_arg;
    int event_sock_arg;
    struct rb_info rb_infos[NUM_RING_BUFS];
    void (*on_ring_buffer_data) (char *ring_name, char *buffer, int buffer_size,
          wifi_ring_buffer_status *status);
    void (*on_alert) (wifi_request_id id, char *buffer, int buffer_size, int err_code);
    struct pkt_stats_s *pkt_stats;

    /* socket pair used to exit from blocking poll*/
    int exit_sockets[2];
    u32 rx_buf_size_allocated;
    u32 rx_buf_size_occupied;
    wifi_ring_buffer_entry *rx_aggr_pkts;
    rx_aggr_stats aggr_stats;
    u32 prev_seq_no;
    // pointer to structure having various gscan_event_handlers
    struct gscan_event_handlers_s *gscan_handlers;
    struct tcp_param_cmd_handler_s *tcp_param_handler;
    /* mutex for the log_handler access*/
    pthread_mutex_t lh_lock;
    /* mutex for the alert_handler access*/
    pthread_mutex_t ah_lock;
    u32 firmware_bus_max_size;
    bool fate_monitoring_enabled;
    packet_fate_monitor_info *pkt_fate_stats;
    /* mutex for the packet fate stats shared resource protection */
    pthread_mutex_t pkt_fate_stats_lock;
    struct rssi_monitor_event_handler_s *rssi_handlers;
    struct radio_event_handler_s *radio_handlers;
    wifi_capa capa;
    void *cldctx;
    bool apf_enabled;
    bool support_nan_ext_cmd;
    pkt_log_version  pkt_log_ver;
#ifndef TARGET_SUPPORTS_WEARABLES
    /* Interface combination matrix */
    wifi_iface_concurrency_matrix iface_comb_matrix;
#endif /* TARGET_SUPPORTS_WEARABLES */
    qca_wlan_vendor_sar_version sar_version;
    struct wpa_secure_nan *secure_nan;
} hal_info;

typedef struct {
    bool radar_detected;
    u32 freq;
    u64 clock_boottime;
} radar_history_result;

static inline void wifi_put_le16(u8 *a, u16 val) {
    a[1] = val >> 8;
    a[0] = val & 0xff;
}

wifi_error wifi_register_handler(wifi_handle handle, int cmd, nl_recvmsg_msg_cb_t func, void *arg);
wifi_error wifi_register_vendor_handler(wifi_handle handle,
            uint32_t id, int subcmd, nl_recvmsg_msg_cb_t func, void *arg);

void wifi_unregister_handler(wifi_handle handle, int cmd);
void wifi_unregister_vendor_handler(wifi_handle handle, uint32_t id, int subcmd);

interface_info *getIfaceInfo(wifi_interface_handle);
wifi_handle getWifiHandle(wifi_interface_handle handle);
hal_info *getHalInfo(wifi_handle handle);
hal_info *getHalInfo(wifi_interface_handle handle);
wifi_handle getWifiHandle(hal_info *info);
wifi_interface_handle getIfaceHandle(interface_info *info);
wifi_error initializeGscanHandlers(hal_info *info);
wifi_error cleanupGscanHandlers(hal_info *info);
wifi_error initializeRSSIMonitorHandler(hal_info *info);
wifi_error cleanupRSSIMonitorHandler(hal_info *info);
wifi_error initializeRadioHandler(hal_info *info);
wifi_error cleanupRadioHandler(hal_info *info);

lowi_cb_table_t *getLowiCallbackTable(u32 requested_lowi_capabilities);

wifi_error wifi_start_sending_offloaded_packet(wifi_request_id id,
        wifi_interface_handle iface, u8 *ip_packet, u16 ip_packet_len,
        u8 *src_mac_addr, u8 *dst_mac_addr, u32 period_msec);
wifi_error wifi_stop_sending_offloaded_packet(wifi_request_id id,
        wifi_interface_handle iface);
wifi_error wifi_start_rssi_monitoring(wifi_request_id id, wifi_interface_handle
        iface, s8 max_rssi, s8 min_rssi, wifi_rssi_event_handler eh);
wifi_error wifi_stop_rssi_monitoring(wifi_request_id id, wifi_interface_handle iface);
wifi_error wifi_set_radio_mode_change_handler(wifi_request_id id, wifi_interface_handle
        iface, wifi_radio_mode_change_handler eh);
wifi_error mapKernelErrortoWifiHalError(int kern_err);
void wifi_cleanup_dynamic_ifaces(wifi_handle handle);
wifi_error wifi_virtual_interface_create(wifi_handle handle, const char* ifname,
                                         wifi_interface_type iface_type);
wifi_error wifi_virtual_interface_delete(wifi_handle handle, const char* ifname);
wifi_error wifi_get_radar_history(wifi_interface_handle handle,
        radar_history_result *resultBuf, int resultBufSize, int *numResults);
wifi_error wifi_disable_next_cac(wifi_interface_handle handle);

wifi_error wifi_get_supported_radio_combinations_matrix(
        wifi_handle handle, u32 max_size, u32 *size,
        wifi_radio_combination_matrix *radio_combination_matrix);
// some common macros

#define min(x, y)       ((x) < (y) ? (x) : (y))
#define max(x, y)       ((x) > (y) ? (x) : (y))

#define REQUEST_ID_MAX 1000
#define REQUEST_ID_U8_MAX 255
#define get_requestid() ((arc4random()%REQUEST_ID_MAX) + 1)
#define get_requestid_u8() ((arc4random()%REQUEST_ID_U8_MAX) + 1)
#define WAIT_TIME_FOR_SET_REG_DOMAIN 50000
#define ITER_COUNT_FOR_SET_REG_DOMAIN 10

#ifndef UNUSED
#define UNUSED(x)    (void)(x)
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
void hexdump(void *bytes, u16 len);
u8 get_rssi(u8 rssi_wo_noise_floor);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

