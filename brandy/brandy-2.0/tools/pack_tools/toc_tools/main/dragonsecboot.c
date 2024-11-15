/*
**********************************************************************************************************************
*											        eGon
*						           the Embedded GO-ON Bootloader System
*									       eGON arm boot sub-system
*
*						  Copyright(C), 2006-2014, Allwinner Technology Co., Ltd.
*                                           All Rights Reserved
*
* File    :
*
* By      : Jerry
*
* Version : V2.00
*
* Date	  :
*
* Descript:
**********************************************************************************************************************
*/

#include "common.h"
#include "include.h"

int root_rollback_used;
aw_nvc_ext_t nvc_ext;

int32_t toc0_item_name[ENUM_TOC0_ITEM_MAX] = {
	[ENUM_TOC0_ITEM_CTERIF] = ITEM_NAME_SBROMSW_CERTIF,
	[ENUM_TOC0_ITEM_FW] = ITEM_NAME_SBROMSW_FW,
	[ENUM_TOC0_ITEM_KEY] = ITEM_NAME_SBROMSW_KEY
};

char *toc0_item_cfg[ENUM_TOC0_ITEM_MAX] = {
	[ENUM_TOC0_ITEM_CTERIF] = NULL,
	[ENUM_TOC0_ITEM_FW] = "toc",
	[ENUM_TOC0_ITEM_KEY] = NULL
};

static void usage(void)
{
	printf(" -toc0 cfg_file keypath                  create a toc0 file by the sbromsw file\n");
	printf(" -toc1 cfg_file keypath cnfbase          create a toc1 file by the cfg_file\n");
	printf(" -toc1 cfg_file keypath cnfbase version  create a toc1 file by the cfg_file\n");
	printf(" -key  cfg_file keypath                  create all keys by the cfg_file\n");
	printf(" -pack cfg_file                          create a package file by the cfg_file without certif\n");
	printf(" -rotpk rsa-key-pair.file rootpk.bin     create root public key hash file from rsa key pair\n");

	return;
}

void sunxi_dump(void *addr, unsigned int size)
{
	int i, j;
	char *buf = (char *)addr;

	for (j = 0; j < size; j += 16) {
		for (i = 0; i < 16; i++) {
			printf("%02x ", buf[j+i] & 0xff);
		}
		printf("\n");
	}
	printf("\n");
	return ;
}

enum TOC0_CORRUPTED_TYPE_EN toc0_corrupted_type = ALL_GOOD;

int main(int argc, char *argv[])
{
	char *cmd = argv[1];
	char  cmdline[1024];

	if (argc < 3) {
		printf("Dragon Secure Boot: not enough parameters\n");

		return -1;
	}
    root_rollback_used = 0;
    nvc_ext.nvc = 0;
    nvc_ext.oid = AW_EXTENSION_NVC_OID;

	if (!strcmp(cmd, "-key")) {
		char keypath[MAX_PATH] = "";
		int ret;

		if (argc != 4) {
			printf("Dragon Secure Boot: -key: invalid paramter counts\n");
			usage();

			return -1;
		}

		GetFullPath(keypath, argv[3]);
		printf("keypath=%s\n", keypath);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "rm -rf %s", keypath);
		system(cmdline);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "mkdir -p %s", keypath);
		system(cmdline);

		ret = dragoncreatekey(argv[2], keypath);
		if (ret < 0) {
			return -1;
		}
		ret = dragon_create_rotpk(argv[2], keypath);
		if (ret < 0) {
			return -1;
		}

		return 0;
	} else if (strcmp(argv[1], "-toc1") == 0) {
		char tmpdirpath[MAX_PATH];
		char toc1path[MAX_PATH] = "";
		char keypath[MAX_PATH] = "";
		char cnfpath[MAX_PATH] = "";
		char versionpath[MAX_PATH] = "";
		int  main_version = 0, sub_version = 0;

		toc_descriptor_t  *toc1;

		if (argc < 5) {
			printf("Dragon Secure Boot: -toc1: invalid paramter counts\n");

			usage();
			return -1;
		}

		GetFullPath(keypath, argv[3]);
		GetFullPath(cnfpath, argv[4]);
		/*printf("keypath=%s\n", keypath);*/
		/*printf("cnfpath=%s\n", cnfpath);*/

		memset(tmpdirpath, 0, MAX_PATH);
		GetFullPath(tmpdirpath, CERTPATH_CONST);
		/*printf("certpath=%s\n", tmpdirpath);*/

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "rm -rf %s", tmpdirpath);
		system(cmdline);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "mkdir -p %s", tmpdirpath);
		system(cmdline);

		memset(tmpdirpath, 0, MAX_PATH);
		GetFullPath(tmpdirpath, CNFPATH_CONST);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "rm -rf %s", tmpdirpath);
		system(cmdline);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "mkdir -p %s", tmpdirpath);
		system(cmdline);

		toc1 = (toc_descriptor_t *)malloc(TOC1_CONFIG_MAX * sizeof(toc_descriptor_t));
		if (toc1 == NULL) {
			printf("main -toc1 cant malloc memory to store toc1 config info\n");

			return -1;
		}
		memset(toc1, 0, TOC1_CONFIG_MAX * sizeof(toc_descriptor_t));

		if (create_cert_for_toc1(argv[2], "toc1", toc1, keypath, cnfpath)) {
			printf("create cnf failed\n");
			free(toc1);

			return -1;
		}

		if (argc == 6) {
			GetFullPath(versionpath, argv[5]);
			if (sboot_get_version(&main_version, &sub_version, versionpath)) {
				printf("get version failed\n");
				free(toc1);

				return -1;
			}
		}


		GetFullPath(toc1path, TOC1_CONST_NAME);
		if (createtoc1(toc1, toc1path, main_version, sub_version)) {
			printf("create cnf failed\n");
			free(toc1);

			return -1;
		}
	} else if (strcmp(argv[1], "-mips") == 0) {
		char tmpdirpath[MAX_PATH];
		char keypath[MAX_PATH] = "";
		char cnfpath[MAX_PATH] = "";

		toc_descriptor_t  *mips;

		if (argc < 5) {
			printf("Dragon Secure Boot: -mips: invalid paramter counts\n");

			usage();
			return -1;
		}

		GetFullPath(keypath, argv[3]);
		GetFullPath(cnfpath, argv[4]);
		/*printf("keypath=%s\n", keypath);*/
		/*printf("cnfpath=%s\n", cnfpath);*/

		memset(tmpdirpath, 0, MAX_PATH);
		GetFullPath(tmpdirpath, "mips/cert");
		printf("certpath=%s\n", tmpdirpath);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "rm -rf %s", tmpdirpath);
		system(cmdline);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "mkdir -p %s", tmpdirpath);
		system(cmdline);

		memset(tmpdirpath, 0, MAX_PATH);
		GetFullPath(tmpdirpath, "mips/cnf");

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "rm -rf %s", tmpdirpath);
		system(cmdline);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "mkdir -p %s", tmpdirpath);
		system(cmdline);

		mips = (toc_descriptor_t *)malloc(TOC1_CONFIG_MAX * sizeof(toc_descriptor_t));
		if (mips == NULL) {
			printf("main -mips cant malloc memory to store mips config info\n");

			return -1;
		}
		memset(mips, 0, TOC1_CONFIG_MAX * sizeof(toc_descriptor_t));

		if (create_cert_for_toc1(argv[2], "mips", mips, keypath, cnfpath)) {
			printf("create cnf failed\n");
			free(mips);

			return -1;
		}

	} else if (strcmp(argv[1], "-toc0") == 0) {
		char tmpdirpath[MAX_PATH];
		char toc0path[MAX_PATH] = "";
		char keypath[MAX_PATH] = "";
		char versionpath[MAX_PATH] = "";
		int  main_version = 0;
		int sub_version = 0;
		toc_descriptor_t  *toc0;
		toc_key_item_descriptor_t *toc0_key_item = NULL;
		int ret = 0;

		if (argc <= 3 || argc >= 7) {
			printf("Dragon Secure Boot: -toc0: invalid paramter counts\n");
			usage();

			return -1;
		}
		if (argc >= 5) {
			printf("toc0 get nvc for version\n");
			GetFullPath(versionpath, argv[4]);
			if (sboot_get_version(&main_version, &sub_version, versionpath)) {
				printf("get version failed\n");
				return -1;
			}
		}
		/*build corrupted toc0*/
		if (argc >= 6) {
			if (strstr(argv[5], "bad_hash") != 0) {
				toc0_corrupted_type = BAD_HASH;
			} else if (strstr(argv[5], "bad_mod") != 0) {
				toc0_corrupted_type = BAD_MOD;
			} else if (strstr(argv[5], "bad_pk") != 0) {
				toc0_corrupted_type = BAD_PK;
			} else if (strstr(argv[5], "bad_sign") != 0) {
				toc0_corrupted_type = BAD_SIGN;
			}
		}
		GetFullPath(keypath, argv[3]);

		memset(tmpdirpath, 0, MAX_PATH);
		GetFullPath(tmpdirpath, TOC0PATH_CONST);
		printf("certpath=%s\n", tmpdirpath);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "rm -rf %s", tmpdirpath);
		system(cmdline);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "mkdir -p %s", tmpdirpath);
		system(cmdline);

		if (prode_toc_key_ladder(argv[2])) {
			printf("enter key_ladder \n");
			toc0_key_item = (toc_key_item_descriptor_t *)malloc(sizeof(toc_key_item_descriptor_t));
			if (toc0_key_item == NULL) {
				printf("cant malloc memory for toc_key_item_descriptor_t\n");

				return -1;
			}
			memset(toc0_key_item, 0, sizeof(toc_key_item_descriptor_t));

			ret = create_all_file_for_toc0(argv[2], toc0_key_item, keypath);
			if (ret <= 0) {
				printf("create_all_file_for_toc0 is err\n");
				return -1;
			}

			GetFullPath(toc0path, TOC0_CONST_NAME);
			if (create_toc0_for_key_ladder(toc0_key_item, TOC0_CONST_NAME)) {
				printf("create_toc0_for_key_ladder failed\n");
				free(toc0_key_item);

				return -1;
			}
		} else {
			toc0 = (toc_descriptor_t *)malloc(sizeof(toc_descriptor_t));
			if (toc0 == NULL) {
				printf("main -toc0 cant malloc memory to store toc0 config info\n");

				return -1;
			}
			memset(toc0, 0, sizeof(toc_descriptor_t));

			if (create_cert_for_toc0(argv[2], toc0, keypath)) {
				printf("create cert failed\n");
				free(toc0);

				return -1;
			}
			GetFullPath(toc0path, TOC0_CONST_NAME);
			if (createtoc0(toc0, toc0path)) {
				printf("create toc0 failed\n");
				free(toc0);

				return -1;
			}
		}
	}
	else if(strcmp(argv[1], "-pack") == 0)
	{
		char package_path[MAX_PATH]="";
		toc_descriptor_t  *package;

		if(argc != 3)
		{
			printf("Dragon Boot: -pack: invalid paramter counts\n");

			usage();
			return -1;
		}

		package = (toc_descriptor_t *)malloc(PACKAGE_CONFIG_MAX * sizeof(toc_descriptor_t));
		if(package == NULL)
		{
			printf("main -package cant malloc memory to store package config info\n");

			return -1;
		}
		memset(package, 0, PACKAGE_CONFIG_MAX * sizeof(toc_descriptor_t));

		if(createcnf_for_package(argv[2], package))
		{
			printf("create config for package failed\n");
			free(package);

			return -1;
		}
		GetFullPath(package_path, PACKAGE_CONST_NAME);
		if(create_package(package, package_path))
		{
			printf("create package failed\n");
			free(package);

			return -1;
		}
	}
	else if(strcmp(argv[1], "-rotpk") == 0){
		if(argc != 4) {
			printf("Dragon Boot: -rotpk: invalid paramter counts\n");
			usage();
			return -1 ;
		}

		if( dragon_create_rotpk_from_keypair(argv[2] , argv[3]) <0 ){
			printf("create rotpk from keypair fail\n");
			return -1 ;
		}

	}
	else if(strcmp(argv[1], "-resign_toc0") ==0 ){
		if(argc != 4) {
			printf("Dragon Boot: -split: invalid paramter counts\n");
			usage();
			return -1 ;
		}

		char tmpdirpath[MAX_PATH];
		char toc0path[MAX_PATH]="";
		char keypath[MAX_PATH]="";
		toc_descriptor_t  *toc0;
		char versionpath[MAX_PATH] = "";
		int  main_version = 0;
		int sub_version = 0;

		if (argc <= 3 || argc >= 6)
		{
			printf("Dragon Secure Boot: -toc0: invalid paramter counts\n");
			usage();

			return -1;
		}

		if (argc == 5) {
			printf("resign_toc0 get nvc for version\n");
			GetFullPath(versionpath, argv[4]);
			if (sboot_get_version(&main_version, &sub_version, versionpath)) {
				printf("get version failed\n");
				return -1;
			}
		}
		GetFullPath(keypath, argv[3]);

		memset(tmpdirpath, 0, MAX_PATH);
		GetFullPath(tmpdirpath, TOC0PATH_CONST);
		printf("certpath=%s\n", tmpdirpath);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "rm -rf %s", tmpdirpath);
		system(cmdline);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "mkdir -p %s", tmpdirpath);
		system(cmdline);

		splittoc0(TOC0_CONST_NAME);
		toc0 = (toc_descriptor_t *)malloc(sizeof(toc_descriptor_t));
		if(toc0 == NULL)
		{
			printf("main -resign_toc0 cant malloc memory to store toc0 config info\n");

			return -1;
		}
		memset(toc0, 0, sizeof(toc_descriptor_t));

		if(create_cert_for_toc0(argv[2], toc0, keypath))
		{
			printf("create cnf failed\n");
			free(toc0);

			return -1;
		}
		GetFullPath(toc0path, TOC0_CONST_NAME);
		if(update_toc0_cert(toc0, toc0path))
		{
			printf("create cnf failed\n");
			free(toc0);

			return -1;
		}

	}
	else if(strcmp(argv[1], "-resign_toc1") ==0 ){
		char tmpdirpath[MAX_PATH];
		char keypath[MAX_PATH] = "";
		char cnfpath[MAX_PATH] = "";
		toc_descriptor_t  *toc1;

		if(argc != 5)
		{
			printf("Dragon Secure Boot: -toc1: invalid paramter counts\n");

			usage();
			return -1;
		}

		GetFullPath(keypath, argv[3]);
		GetFullPath(cnfpath, argv[4]);

		//printf("keypath=%s\n", keypath);
		//printf("cnfpath=%s\n", cnfpath);

		memset(tmpdirpath, 0, MAX_PATH);
		GetFullPath(tmpdirpath, CERTPATH_CONST);
		//printf("certpath=%s\n", tmpdirpath);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "rm -rf %s", tmpdirpath);
		system(cmdline);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "mkdir -p %s", tmpdirpath);
		system(cmdline);

		memset(tmpdirpath, 0, MAX_PATH);
		GetFullPath(tmpdirpath, CNFPATH_CONST);
		//printf("cnfpath=%s\n", tmpdirpath);
		memset(cmdline, 0, 1024);
		sprintf(cmdline, "rm -rf %s", tmpdirpath);
		system(cmdline);

		memset(cmdline, 0, 1024);
		sprintf(cmdline, "mkdir -p %s", tmpdirpath);
		system(cmdline);

		toc1 = (toc_descriptor_t *)malloc(TOC1_CONFIG_MAX * sizeof(toc_descriptor_t));
		if(toc1 == NULL)
		{
			printf("main -resign_toc1 cant malloc memory to store toc1 config info\n");

			return -1;
		}
		memset(toc1, 0, TOC1_CONFIG_MAX * sizeof(toc_descriptor_t));
		if( splittoc1(TOC1_CONST_NAME) <0 ){
			printf("split uboot from toc1 fail\n");
			free(toc1);
			return -1 ;
		}
		free(toc1);
		return 0 ;

	} else if (strcmp(argv[1], "-split_toc0") == 0) {

		char toc0path[MAX_PATH] = "";

		GetFullPath(toc0path, argv[2]);

		splittoc0_item(toc0path, toc0_item_name[ENUM_TOC0_ITEM_FW], "sboot.bin");

	} else if (strcmp(argv[1], "-split_toc1") == 0) {
		char toc1path[MAX_PATH] = "";

		char all_toc1[1024];
		char *all_toc1_line[16];
		char type_name[32], line_info[256];
		char bin_name[64];
		int i;
		toc_descriptor_t  *toc1;
		toc1 = (toc_descriptor_t *)malloc(TOC1_CONFIG_MAX * sizeof(toc_descriptor_t));
		if (toc1 == NULL) {
			printf("main -toc1 cant malloc memory to store toc1 config info\n");
			free(toc1);
			return -1;
		}

		GetFullPath(toc1path, argv[2]);
		printf("toc0path=%s\n", toc1path);
		if (get_section_data("toc1", all_toc1, argv[3])) {
			if (get_section_data("package", all_toc1, argv[3])) {
				printf("get toc1 and package err in GetPrivateProfileSection\n");
				free(toc1);
				return -1;
			}
		}

		if (GetPrivateProfileAllLine(all_toc1, all_toc1_line)) {
			printf("dragoncreate_toc1_certif err in GetPrivateProfileAllLine\n");

			free(toc1);
			return -1;
		}

		for (i = 0; i < 16; i++) {
			if (all_toc1_line[i]) {
				memset(type_name, 0, 32);
				memset(line_info, 0, 256);

				GetPrivateProfileLineInfo(all_toc1_line[i], type_name, line_info);

				memset(bin_name, 0, 64);
				GetPrivateEachItem(line_info, toc1[i].item, bin_name, NULL);
				printf("toc1[%d].item=%s bin_name=%s, type_name = %s\n",
					i, toc1[i].item, bin_name, type_name);
				splittoc1_item(toc1path, toc1[i].item, bin_name);
			}
		}

		free(toc1);
		return 0 ;

	} else {
		usage();
		return -1;
	}

	return 0;
}
