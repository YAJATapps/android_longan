/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 */

#ifndef _SS_H_
#define _SS_H_

#include <config.h>
#include <arch/cpu.h>

#define SS_N_BASE SUNXI_SS_BASE /* non security */
#define SS_S_BASE (SUNXI_SS_BASE + 0x800) /* security */

#define SS_TDQ (SS_N_BASE + 0x00 + 0x800 * ss_base_mode)
#define SS_ICR (SS_N_BASE + 0x08 + 0x800 * ss_base_mode)
#define SS_ISR (SS_N_BASE + 0x0C + 0x800 * ss_base_mode)
#define SS_TLR (SS_N_BASE + 0x10 + 0x800 * ss_base_mode)
#define SS_TSR (SS_N_BASE + 0x14 + 0x800 * ss_base_mode)
#define SS_ERR (SS_N_BASE + 0x18 + 0x800 * ss_base_mode)
#define SS_TPR (SS_N_BASE + 0x1C + 0x800 * ss_base_mode)
/* #define SS_VER          (SS_N_BASE + 0x90 + 0x800*ss_base_mode) */
#define SS_VER (SS_N_BASE + 0x90)

/* security */
#define SS_S_TDQ (SS_S_BASE + 0x00)
#define SS_S_CTR (SS_S_BASE + 0x04)
#define SS_S_ICR (SS_S_BASE + 0x08)
#define SS_S_ISR (SS_S_BASE + 0x0C)
#define SS_S_TLR (SS_S_BASE + 0x10)
#define SS_S_TSR (SS_S_BASE + 0x14)
#define SS_S_ERR (SS_S_BASE + 0x18)
#define SS_S_TPR (SS_S_BASE + 0x1C)

/* non security */
#define SS_N_TDQ (SS_N_BASE + 0x00)
#define SS_N_ICR (SS_N_BASE + 0x08)
#define SS_N_ISR (SS_N_BASE + 0x0C)
#define SS_N_TLR (SS_N_BASE + 0x10)
#define SS_N_TSR (SS_N_BASE + 0x14)
#define SS_N_ERR (SS_N_BASE + 0x18)
#define SS_N_TPR (SS_N_BASE + 0x1C)

#define SHA1_160_MODE 0
#define SHA2_256_MODE 1

#define ALG_SHA256    (0x13)
#define ALG_RSA	      (0x20)
#define TRNG_BYTE_LEN (32)

/*ctrl*/
#define CHN  (0) /*channel id*/
#define IVE  (8)
#define LPKG (12) /*last package*/
#define DLAV (13) /*data length valid当最后一个包,需要设为1,*/
#define IE   (16)

#define MD5    (0)
#define SHA1   (1)
#define SHA244 (2)
#define SHA256 (3)
#define SHA384 (4)
#define SHA512 (5)
#define SM3    (6)

#define TRNG (2)

/*cmd*/
#define HASH_SEL 0
#define HME	 4
#define RGB_SEL	 8
#define SUB_CMD	 16


/*CE_ISR*/
#define SUCCESS 0x1
#define FAIL	0x2
#define CLEAN	0x3

#define CHANNEL_0 0
#define CHANNEL_1 1
#define CHANNEL_2 2
#define CHANNEL_3 3

#pragma pack(push, 1)
typedef struct sg {
	u8 source_addr[5];
	u8 dest_addr[5];
	u8 pad[2];
	u32 source_len;
	u32 dest_len;
} sg;

struct other_task_descriptor {
	u32 task_id;
	u32 common_ctl;
	u32 symmetric_ctl;
	u32 asymmetric_ctl;
	u8 key_addr[5];
	u8 iv_addr[5];
	u8 ctr_addr[5];
	u8 pad;
	u32 data_len;
	struct sg sg[8];
	u8 next_sg_addr[5];
	u8 next_task_addr[5];
	u8 pad2[2];
	u32 reserve[3];
};

struct hash_task_descriptor {
	u32 ctrl;
	u32 cmd;
	u8 data_toal_len_addr[5];
	u8 hmac_prng_key_addr[5];
	u8 iv_addr[5];
	u8 pad;
	struct sg sg[8];
	u8 next_sg_addr[5];
	u8 next_task_addr[5];
	u8 pad2[2];
	u32 reserve[3];
};
#pragma pack(pop)

struct ecc_verify_params{
	u8 *p;
	u8 *a;
	u8 *n;
	u8 *gx;
	u8 *gy;
	u8 *r;
	u8 *s;
	u8 *qx;
	u8 *qy;
};
struct ecc_curve_param_t {
	u8 a[32];
	u8 b[32];
	u8 n[32];
	u8 p[32];
	u8 gx[32];
	u8 gy[32];
};

struct ecc_pubkey_t {
	u8 qx[32];
	u8 qy[32];
};

struct ecc_signature_t {
	u8 r[32];
	u8 s[32];
};

extern const struct ecc_curve_param_t p256_param;

void sunxi_ss_open(void);
void sunxi_ss_close(void);
int sunxi_sha_calc(u8 *dst_addr, u32 dst_len, u8 *src_addr, u32 src_len);

s32 sunxi_rsa_calc(u8 *n_addr, u32 n_len, u8 *e_addr, u32 e_len, u8 *dst_addr,
		   u32 dst_len, u8 *src_addr, u32 src_len);

#endif /*  #ifndef _SS_H_  */
