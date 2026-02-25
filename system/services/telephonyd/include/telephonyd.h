/*
 * telephonyd.h - LumiOS Telephony Service / 电话服务
 *
 * Manages cellular modem via RIL/AT commands, handles calls, SMS, and data.
 * 通过 RIL/AT 命令管理蜂窝基带，处理通话、短信和数据。
 */

#ifndef TELEPHONYD_H
#define TELEPHONYD_H

#include <stdbool.h>
#include <stdint.h>

#define TELEPHONYD_VERSION "0.1.0"
#define TELEPHONYD_SOCKET  "/run/telephonyd.sock"

/* === SIM state / SIM 卡状态 === */
typedef enum {
    SIM_ABSENT = 0,
    SIM_NOT_READY,
    SIM_PIN_REQUIRED,
    SIM_PUK_REQUIRED,
    SIM_READY,
    SIM_ERROR,
} sim_state_t;

/* === Network registration / 网络注册状态 === */
typedef enum {
    NET_REG_NOT_REGISTERED = 0,
    NET_REG_HOME,
    NET_REG_SEARCHING,
    NET_REG_DENIED,
    NET_REG_UNKNOWN,
    NET_REG_ROAMING,
} net_reg_state_t;

/* === Radio technology / 无线技术 === */
typedef enum {
    RAT_UNKNOWN = 0,
    RAT_GSM,
    RAT_EDGE,
    RAT_UMTS,
    RAT_HSPA,
    RAT_HSPAP,
    RAT_LTE,
    RAT_LTE_CA,
    RAT_NR,           /* 5G NR */
    RAT_NR_NSA,       /* 5G Non-Standalone */
} radio_tech_t;

/* === Signal info / 信号信息 === */
typedef struct {
    radio_tech_t tech;
    int          rssi;          /* dBm */
    int          rsrp;          /* LTE: dBm */
    int          rsrq;          /* LTE: dB */
    int          sinr;          /* LTE: dB */
    int          level;         /* 0-5 bars */
} signal_info_t;

/* === SIM card info / SIM 卡信息 === */
typedef struct {
    sim_state_t  state;
    char         iccid[24];
    char         imsi[16];
    char         operator_name[64];
    char         operator_mcc[4];    /* Mobile Country Code */
    char         operator_mnc[4];    /* Mobile Network Code */
    char         phone_number[32];
    int          slot;               /* 0 or 1 for dual-SIM */
} sim_info_t;

/* === Modem info / 基带信息 === */
typedef struct {
    char         device_path[64];    /* /dev/ttyUSB0 */
    char         manufacturer[64];
    char         model[64];
    char         firmware[64];
    char         imei[20];
    bool         powered;
    bool         online;
} modem_info_t;

/* === Call state (from modem perspective) / 通话状态 === */
typedef enum {
    TCALL_IDLE = 0,
    TCALL_DIALING,
    TCALL_ALERTING,
    TCALL_INCOMING,
    TCALL_ACTIVE,
    TCALL_HELD,
    TCALL_WAITING,
    TCALL_RELEASED,
} tcall_state_t;

typedef struct {
    tcall_state_t state;
    char          number[32];
    int           call_id;
    bool          multiparty;
} tcall_t;

/* === SMS PDU / 短信 PDU === */
typedef struct {
    char     sender[32];
    char     recipient[32];
    char     body[512];
    uint64_t timestamp;
    int      pdu_type;         /* 0=deliver, 1=submit */
} sms_pdu_t;

/* === Data connection / 数据连接 === */
typedef struct {
    bool         active;
    char         apn[64];
    char         ip[16];
    char         gateway[16];
    char         dns1[16];
    char         dns2[16];
    radio_tech_t tech;
    uint64_t     rx_bytes;
    uint64_t     tx_bytes;
} data_conn_t;

/* === Telephonyd instance / 电话服务实例 === */
typedef struct {
    modem_info_t    modem;
    sim_info_t      sim[2];          /* dual-SIM */
    int             active_sim;
    signal_info_t   signal;
    net_reg_state_t reg_state;
    tcall_t         calls[8];
    int             num_calls;
    data_conn_t     data;
    bool            airplane_mode;
    bool            running;
    int             modem_fd;        /* serial port fd */
} telephonyd_t;

/* === Function declarations / 函数声明 === */

/* telephonyd.c - Core daemon */
int  telephonyd_init(telephonyd_t *t);
void telephonyd_run(telephonyd_t *t);
void telephonyd_shutdown(telephonyd_t *t);

/* modem.c - Modem management / 基带管理 */
int  modem_open(telephonyd_t *t, const char *device);
int  modem_close(telephonyd_t *t);
int  modem_send_at(telephonyd_t *t, const char *cmd, char *resp, size_t resp_len);
int  modem_power(telephonyd_t *t, bool on);
int  modem_get_info(telephonyd_t *t);

/* sim.c - SIM management / SIM 卡管理 */
int  sim_get_state(telephonyd_t *t, int slot);
int  sim_enter_pin(telephonyd_t *t, int slot, const char *pin);
int  sim_get_info(telephonyd_t *t, int slot);
int  sim_switch(telephonyd_t *t, int slot);

/* network.c - Network registration / 网络注册 */
int  network_register(telephonyd_t *t);
int  network_get_signal(telephonyd_t *t, signal_info_t *sig);
int  network_get_operator(telephonyd_t *t, char *name, size_t len);
int  network_set_preferred_tech(telephonyd_t *t, radio_tech_t tech);
int  network_set_airplane(telephonyd_t *t, bool enabled);

/* call.c - Call control / 通话控制 */
int  call_originate(telephonyd_t *t, const char *number);
int  call_answer(telephonyd_t *t, int call_id);
int  call_hangup(telephonyd_t *t, int call_id);
int  call_hold(telephonyd_t *t, int call_id);
int  call_dtmf(telephonyd_t *t, char digit);

/* sms.c - SMS handling / 短信处理 */
int  sms_send(telephonyd_t *t, const char *number, const char *body);
int  sms_read_incoming(telephonyd_t *t, sms_pdu_t *pdu);

/* data.c - Mobile data / 移动数据 */
int  data_connect(telephonyd_t *t, const char *apn);
int  data_disconnect(telephonyd_t *t);
int  data_get_status(telephonyd_t *t, data_conn_t *conn);

#endif /* TELEPHONYD_H */
