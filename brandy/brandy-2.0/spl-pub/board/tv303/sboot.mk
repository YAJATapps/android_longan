
#
#config file for sun50iw12
#
#stroage
FILE_EXIST=$(shell if [ -f $(TOPDIR)/board/$(PLATFORM)/common.mk ]; then echo yes; else echo no; fi;)
ifeq (x$(FILE_EXIST),xyes)
include $(TOPDIR)/board/$(PLATFORM)/common.mk
else
include $(TOPDIR)/board/$(CP_BOARD)/common.mk
endif

MODULE=sboot

CFG_SUNXI_MMC =y
CFG_SUNXI_NAND =y
CFG_SUNXI_SPINAND =y
CFG_SUNXI_CE_23 =y
CFG_SUNXI_SMC_30=y

CFG_SUNXI_SBOOT =y

CFG_SUNXI_ITEM_HASH =y

CFG_SUNXI_DMA_DEFAULT_NS =y
CFG_SUNXI_DMA_CHANNEL_MASK =0xFF
