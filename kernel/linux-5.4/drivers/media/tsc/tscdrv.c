/*
 * drivers/media/tsc/tscdrv.c
 * (C) Copyright 2010-2015
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * csjamesdeng <csjamesdeng@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/rmap.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <asm-generic/gpio.h>
#include <asm/current.h>
#include <linux/platform_device.h>
#include "dvb_drv_sunxi.h"
#include "tscdrv.h"
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/consumer.h>
#include <linux/reset.h>

#define TSC_MSG(fmt, arg...) pr_warn("[tsc]: "fmt, ##arg)
#define TSC_DBG(fmt, arg...) \
	pr_debug("[tsc]: %s()%d - "fmt, __func__, __LINE__, ##arg)
#define TSC_ERR(fmt, arg...) \
	pr_warn("[tsc]: %s()%d - "fmt, __func__, __LINE__, ##arg)
#define TSC_INFO(fmt, arg...) \
	pr_warn("[tsc]: %s()%d - "fmt, __func__, __LINE__, ##arg)

static struct tsc_dev *tsc_devp;
static struct of_device_id sunxi_tsc_match[] = {
	{ .compatible = "allwinner,sun8i-tsc",},
	{ .compatible = "allwinner,sun50i-tsc",},
	{}
};

#define TSC_MODULE_CLK_RATE 120000000
#define PRINTK_IOMMU_ADDR   1

MODULE_DEVICE_TABLE(of, sunxi_tsc_match);

static DECLARE_WAIT_QUEUE_HEAD(wait_proc);

struct tsc_kernel_mm {
	struct aw_mem_list_head 	i_list;
	size_t				size;
	unsigned long			iommu_addr;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	struct sg_table			*sgt;
	int				fd;
};

int sunxi_tsc_enable_hw_clk(void)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&tsc_devp->lock, flags);

	reset_control_deassert(tsc_devp->reset);
	if (clk_prepare_enable(tsc_devp->mclk)) {
		TSC_MSG("enable mclk failed.\n");
		ret = -EFAULT;
		goto enable_out;
	}

enable_out:
	spin_unlock_irqrestore(&tsc_devp->lock, flags);
	return ret;
}

int sunxi_tsc_disable_hw_clk(void)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&tsc_devp->lock, flags);
	if ((NULL == tsc_devp->mclk)
		|| (IS_ERR(tsc_devp->mclk))) {
		TSC_MSG("mclk is invalid!\n");
		ret = -EFAULT;
	} else {
		clk_disable_unprepare(tsc_devp->mclk);
		reset_control_assert(tsc_devp->reset);
	}

	spin_unlock_irqrestore(&tsc_devp->lock, flags);
	return ret;
}

/*
 * interrupt service routine
 * To wake up wait queue
 */
static irqreturn_t sunxi_tsc_irq_handle(int irq, void *dev_id)
{
	struct iomap_para addrs = tsc_devp->iomap_addrs;
	unsigned long tsc_int_status_reg;
	unsigned long tsc_int_ctrl_reg;
	unsigned int  tsc_status;
	unsigned int  tsc_interrupt_enable;

	tsc_int_ctrl_reg = (unsigned long)(addrs.regs_macc + 0x80 + 0x08);
	tsc_int_status_reg = (unsigned long)(addrs.regs_macc + 0x80 + 0x18);
	tsc_interrupt_enable = ioread32((void *)(tsc_int_ctrl_reg));
	tsc_status = ioread32((void *)(tsc_int_status_reg));
	iowrite32(tsc_interrupt_enable, (void *)(tsc_int_ctrl_reg));
	iowrite32(tsc_status, (void *)(tsc_int_status_reg));
	tsc_devp->irq_flag = 1;
	wake_up_interruptible(&wait_proc);

	return IRQ_HANDLED;
}

static void sunxi_tsc_close_all_filters(struct tsc_dev *devp)
{
	int i;
	unsigned int value = 0;
	struct iomap_para addrs = tsc_devp->iomap_addrs;

	/*close tsf0*/
	iowrite32(0, (void *)(addrs.regs_macc + 0x80 + 0x10));
	iowrite32(0, (void *)(addrs.regs_macc + 0x80 + 0x30));
	for (i = 0; i < 32; i++) {
		iowrite32(i, (void *)(addrs.regs_macc + 0x80 + 0x3c));
		value = (0<<16 | 0x1fff);
		iowrite32(value, (void *)(addrs.regs_macc + 0x80 + 0x4c));
	}
}

static int sunxi_tsc_select_gpio_state(struct pinctrl *pctrl, char *name)
{
	int ret = 0;
	struct pinctrl_state *pctrl_state = NULL;

	pctrl_state = pinctrl_lookup_state(pctrl, name);
	if (IS_ERR(pctrl_state)) {
		TSC_MSG("pinctrl lookup state(%s) failed!\n", name);
		return -1;
	}

	ret = pinctrl_select_state(pctrl, pctrl_state);
	if (ret < 0)
		TSC_MSG("pintrcl select state(%s) failed\n", name);

	return ret;
}


static int sunxi_tsc_request_gpio(struct platform_device *pdev)
{
	int ret = 0;
#ifdef CONFIG_ARCH_SUN50IW2
	if ((tsc_devp->port_config.port0config &&
			tsc_devp->port_config.port1config)
		|| (tsc_devp->port_config.port2config &&
			tsc_devp->port_config.port3config)) {
		TSC_MSG("port config error!\n");
		return -EINVAL;
	}
#endif
	if (tsc_devp->port_config.port0config
		|| tsc_devp->port_config.port1config
		|| tsc_devp->port_config.port2config
		|| tsc_devp->port_config.port3config) {
		if (!tsc_devp->pinctrl) {
			tsc_devp->pinctrl = devm_pinctrl_get(&pdev->dev);
			if (IS_ERR_OR_NULL(tsc_devp->pinctrl)) {
				TSC_MSG("request pinctrl handle  failed!\n");
				return -EINVAL;
			}
		}
	}
	if (tsc_devp->port_config.port0config) {
		ret = sunxi_tsc_select_gpio_state(tsc_devp->pinctrl,
					"ts0-default");
		if (ret)
			TSC_MSG("set gpio default err!\n");
	}
	if (tsc_devp->port_config.port1config) {
		ret = sunxi_tsc_select_gpio_state(tsc_devp->pinctrl,
					"ts1-default");
		if (ret)
			TSC_MSG("set gpio default err!\n");
	}
	if (tsc_devp->port_config.port2config) {
		ret = sunxi_tsc_select_gpio_state(tsc_devp->pinctrl,
					"ts2-default");
		if (ret)
			TSC_MSG("set gpio default err!\n");
	}
	if (tsc_devp->port_config.port3config) {
		ret = sunxi_tsc_select_gpio_state(tsc_devp->pinctrl,
					"ts3-default");
		if (ret)
			TSC_MSG("set gpio default err!\n");
	}

	return ret;
}

static void sunxi_tsc_release_gpio(void)
{
	devm_pinctrl_put(tsc_devp->pinctrl);
	tsc_devp->pinctrl = NULL;
}

static int sunxi_tsc_disable_gpio(void)
{
	int ret = 0;
	if (tsc_devp->port_config.port0config) {
		ret = sunxi_tsc_select_gpio_state(tsc_devp->pinctrl,
					"ts0-sleep");
		if (ret) {
			TSC_ERR("select sleep state failed!\n");
			return ret;
		}
	}
	if (tsc_devp->port_config.port1config) {
		ret = sunxi_tsc_select_gpio_state(tsc_devp->pinctrl,
					"ts1-sleep");
		if (ret) {
			TSC_ERR("select sleep state failed!\n");
			return ret;
		}
	}
	if (tsc_devp->port_config.port2config) {
		ret = sunxi_tsc_select_gpio_state(tsc_devp->pinctrl,
					"ts2-sleep");
		if (ret) {
			TSC_ERR("select sleep state failed!\n");
			return ret;
		}

	}
	if (tsc_devp->port_config.port3config) {
		ret = sunxi_tsc_select_gpio_state(tsc_devp->pinctrl,
					"ts3-sleep");
		if (ret) {
			TSC_ERR("select sleep state failed!\n");
			return ret;
		}

	}
	return ret;
}

static int sunxi_tsc_hw_clk_init(struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int mclk_rate;
	int ret = 0;

	node = pdev->dev.of_node;
	tsc_devp->pclk = of_clk_get(node, 0);
	if ((!tsc_devp->pclk) || IS_ERR(tsc_devp->pclk)) {
		TSC_MSG("try to get parent pll clk failed!\n");
		ret = -EINVAL;
		return ret;
	}

	tsc_devp->mclk = of_clk_get(node, 1);
	if (!tsc_devp->mclk || IS_ERR(tsc_devp->mclk)) {
		TSC_MSG("get mclk failed.\n");
		ret = -EINVAL;
		return ret;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &mclk_rate);
	if (ret) {
		TSC_MSG("don't set clock-frequency by dts\n");
		mclk_rate = TSC_MODULE_CLK_RATE;
	}

	/* reset tsc module. */
	tsc_devp->reset = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(tsc_devp->reset)) {
		TSC_MSG("get reset fail\n");
		return -EINVAL;
	}

	reset_control_assert(tsc_devp->reset);

	clk_set_parent(tsc_devp->mclk, tsc_devp->pclk);

	if (clk_set_rate(tsc_devp->mclk, mclk_rate) < 0) {
		TSC_MSG("set clk rate failed\n");
		ret = -EINVAL;
	}
	TSC_MSG("set clock rate is %ld\n", clk_get_rate(tsc_devp->mclk));

	if (clk_prepare_enable(tsc_devp->mclk)) {
		TSC_MSG("enable moudule clock failed\n");
		return -EBUSY;
	}

	return clk_get_rate(tsc_devp->mclk);
}

static int sunxi_tsc_get_port_config(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int temp_val = 0;
	struct device_node *node;

	node = pdev->dev.of_node;
	ret = of_property_read_u32(node, "ts0config", &temp_val);
	if (ret < 0) {
		TSC_MSG("ts0config missing or invalid.\n");
		tsc_devp->port_config.port0config = 0;
	} else {
		tsc_devp->port_config.port0config = temp_val;
	}
	ret = of_property_read_u32(node, "ts1config", &temp_val);
	if (ret < 0) {
		TSC_MSG("ts1config missing or invalid.\n");
		tsc_devp->port_config.port1config = 0;
	} else {
		tsc_devp->port_config.port1config = temp_val;
	}
	ret = of_property_read_u32(node, "ts2config", &temp_val);
	if (ret < 0) {
		TSC_MSG("ts2config missing or invalid.\n");
		tsc_devp->port_config.port2config = 0;
	} else {
		tsc_devp->port_config.port2config = temp_val;
	}
	ret = of_property_read_u32(node, "ts3config", &temp_val);
	if (ret < 0) {
		TSC_MSG("ts3config missing or invalid.\n");
		tsc_devp->port_config.port3config = 0;
	} else {
		tsc_devp->port_config.port3config = temp_val;
	}

	if (tsc_devp->port_config.port0config ||
		tsc_devp->port_config.port1config ||
		tsc_devp->port_config.port2config ||
			tsc_devp->port_config.port3config)
		return 0;
	else
		return -EINVAL;
}
/*
 * poll operateion for wait for TS irq
 */
unsigned int tscdev_poll(struct file *filp, struct poll_table_struct *wait)
{
	int mask = 0;

	poll_wait(filp, &tsc_devp->wq, wait);
	if (tsc_devp->irq_flag == 1)
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static int sunxi_tsc_map_ionphys(struct tsc_ionbuf_param *user_ion_param)
{
	struct tsc_kernel_mm *pIommuBuf = NULL;

	pIommuBuf = kmalloc(sizeof(struct tsc_kernel_mm), GFP_KERNEL);
	if (pIommuBuf == NULL) {
		TSC_MSG("kmalloc pIommuBuf error");
		return -1;
	}

	memset(pIommuBuf, 0, sizeof(*pIommuBuf));
	pIommuBuf->fd = user_ion_param->fd;
	pIommuBuf->dma_buf = dma_buf_get(pIommuBuf->fd);
	if (pIommuBuf->dma_buf < 0) {
		TSC_MSG("Get dma_buf error");
		goto exit;
	}

	pIommuBuf->attachment = dma_buf_attach(pIommuBuf->dma_buf, get_device(tsc_devp->platform_dev));
	if (IS_ERR(pIommuBuf->attachment)) {
		TSC_MSG("dma_buf_attach failed\n");
		goto err_buf_put;
	}

	pIommuBuf->sgt = dma_buf_map_attachment(pIommuBuf->attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(pIommuBuf->sgt)) {
		TSC_MSG("dma_buf_map_attachment failed\n");
		goto err_buf_detach;
	}

	pIommuBuf->iommu_addr = sg_dma_address(pIommuBuf->sgt->sgl);
	user_ion_param->ion_phyaddr = (unsigned int)(pIommuBuf->iommu_addr & 0xffffffff);
	#if PRINTK_IOMMU_ADDR
	TSC_MSG("malloc: fd:%d, buf_info:%p, iommu_addr:0x%lx, "
		"attachment:%p, sgt:%p dma_buf:%p, size:%lu\n",
	pIommuBuf->fd,
	pIommuBuf,
	pIommuBuf->iommu_addr,
	pIommuBuf->attachment,
	pIommuBuf->sgt,
	pIommuBuf->dma_buf,
	pIommuBuf->size);
	#endif

	mutex_lock(&tsc_devp->lock_mem);
	aw_mem_list_add_tail(&pIommuBuf->i_list, &tsc_devp->list);
	mutex_unlock(&tsc_devp->lock_mem);

	return 0;

err_buf_detach:
	dma_buf_detach(pIommuBuf->dma_buf, pIommuBuf->attachment);
err_buf_put:
	dma_buf_put(pIommuBuf->dma_buf);
exit:
	kfree(pIommuBuf);
	return -1;
}

static int sunxi_tsc_unmap_ionphys(struct tsc_ionbuf_param *user_ion_param, int unmap_all)
{
	struct tsc_kernel_mm *pIommuBuf = NULL;
	struct aw_mem_list_head *pos;
	struct aw_mem_list_head *next;
	int tmp_fd;
	unsigned int tmp_addr;

	mutex_lock(&tsc_devp->lock_mem);
	aw_mem_list_for_each_safe(pos, next, &tsc_devp->list) {
		pIommuBuf = (struct tsc_kernel_mm *)pos;
		if (unmap_all) {
			tmp_fd = pIommuBuf->fd;
			tmp_addr = pIommuBuf->iommu_addr;
		} else {
			tmp_fd = user_ion_param->fd;
			tmp_addr = user_ion_param->ion_phyaddr;
		}
		if (pIommuBuf->fd == tmp_fd && pIommuBuf->iommu_addr == tmp_addr) {
			#if PRINTK_IOMMU_ADDR
			TSC_MSG("free: fd:%d, buf_info:%p , iommu_addr:0x%lx "
				"attachment:%p, sgt:%p dma_buf:%p, size:%lu\n",
			pIommuBuf->fd,
			pIommuBuf,
			pIommuBuf->iommu_addr,
			pIommuBuf->attachment,
			pIommuBuf->sgt,
			pIommuBuf->dma_buf,
			pIommuBuf->size);
			#endif
			if (pIommuBuf->dma_buf > 0) {
				if (pIommuBuf->attachment > 0) {
					if (pIommuBuf->sgt > 0) {
						dma_buf_unmap_attachment(pIommuBuf->attachment,
									 pIommuBuf->sgt,
									 DMA_BIDIRECTIONAL);
					}
					dma_buf_detach(pIommuBuf->dma_buf, pIommuBuf->attachment);
				}
				dma_buf_put(pIommuBuf->dma_buf);
			}
			aw_mem_list_del(&pIommuBuf->i_list);
			kfree(pIommuBuf);
			if (unmap_all == 0)
				break;
		}
	}
	mutex_unlock(&tsc_devp->lock_mem);

	return 0;
}

/*
 * ioctl function
 */
long tscdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;
	struct intrstatus statusdata;
	int arg_rate;
	unsigned long tsc_pclk_rate;

	ret = 0;
	if (_IOC_TYPE(cmd) != TSCDEV_IOC_MAGIC)
		return -EINVAL;
	if (_IOC_NR(cmd) > TSCDEV_IOC_MAXNR)
		return -EINVAL;

	switch (cmd) {
	case TSCDEV_WAIT_INT:
		ret = wait_event_interruptible_timeout(wait_proc,
						tsc_devp->irq_flag, HZ * 1);
		if (!ret && !tsc_devp->irq_flag) {
			/* case: wait timeout. */
			TSC_MSG("wait interrupt timeout.\n");
			memset(&statusdata, 0, sizeof(statusdata));
		} else {
			/* case: interrupt occured. */
			tsc_devp->irq_flag = 0;
			statusdata.port0chan    = tsc_devp->intstatus.port0chan;
			statusdata.port0pcr     = tsc_devp->intstatus.port0pcr;
		}
		/* copy status data to user. */
		if (copy_to_user((struct intrstatus *)arg,
				&(tsc_devp->intstatus),
			sizeof(struct intrstatus))) {
			return -EFAULT;
		}
		break;

	case TSCDEV_GET_PHYSICS:
		return 0;

	case TSCDEV_ENABLE_INT:
		enable_irq(tsc_devp->irq);
		break;

	case TSCDEV_DISABLE_INT:
		tsc_devp->irq_flag = 1;
		wake_up_interruptible(&wait_proc);
		disable_irq(tsc_devp->irq);
		break;

	case TSCDEV_RELEASE_SEM:
		tsc_devp->irq_flag = 1;
		wake_up_interruptible(&wait_proc);
		break;

	case TSCDEV_GET_CLK:
		if (!tsc_devp->mclk || IS_ERR(tsc_devp->mclk)) {
			TSC_MSG("get tsc clk failed.\n");
			ret = -EINVAL;
		}
		break;

	case TSCDEV_PUT_CLK:

		clk_put(tsc_devp->mclk);

		break;

	case TSCDEV_ENABLE_CLK:
		/* clk_prepare_enable(tsc_devp->mclk); */
		ret = sunxi_tsc_enable_hw_clk();
		if (ret < 0) {
			TSC_ERR("tsc clk enable failed!\n");
			return -EFAULT;
		}
		break;

	case TSCDEV_DISABLE_CLK:
		/* clk_disable_unprepare(tsc_devp->mclk); */
		ret = sunxi_tsc_disable_hw_clk();
		if (ret < 0) {
			TSC_ERR("tsc clk disable failed!\n");
		}
		break;

	case TSCDEV_GET_CLK_FREQ:
		ret = clk_get_rate(tsc_devp->mclk);
		break;

	case TSCDEV_SET_SRC_CLK_FREQ:
		writel(0x1, tsc_devp->iomap_addrs.regs_macc);
		break;

	case TSCDEV_SET_CLK_FREQ:
		arg_rate = (int)arg;
		if (clk_get_rate(tsc_devp->mclk)/1000000 != arg_rate) {
			if (!clk_set_rate(tsc_devp->pclk, arg_rate*1000000)) {
				tsc_pclk_rate = clk_get_rate(tsc_devp->pclk);
				if (clk_set_rate(tsc_devp->mclk, tsc_pclk_rate))
					TSC_MSG("set tsc clock failed!\n");
			} else
				TSC_MSG("set pll4 clock failed!\n");
		}
		ret = clk_get_rate(tsc_devp->mclk);
		break;

	case TSCDEV_GET_IONPHYADDR:
	{
		struct tsc_ionbuf_param user_ion_param;

		if (copy_from_user(&user_ion_param, (void __user *)arg, sizeof(user_ion_param))) {
			TSC_MSG("TSCDEV_GET_IONPHYADDR copy_from_user erro\n");
			return -EFAULT;
		}

		ret = sunxi_tsc_map_ionphys(&user_ion_param);
		if (ret) {
			TSC_MSG("TSCDEV_GET_IONPHYADDR map_ionphys error\n");
			return -EINVAL;
		}

		if (copy_to_user((void __user *)arg, &user_ion_param, sizeof(user_ion_param))) {
			TSC_MSG("TSCDEV_GET_IONPHYADDR  copy_to_user error\n");
			sunxi_tsc_unmap_ionphys(&user_ion_param, 0);
			return -EFAULT;
		}
		break;
	}

	case TSCDEV_RELEASE_IONPHYADDR:
	{
		struct tsc_ionbuf_param user_ion_param;

		if (copy_from_user(&user_ion_param, (void __user *)arg, sizeof(user_ion_param))) {
			TSC_MSG("TSCDEV_RELEASE_IONPHYADDR copy_from_user error\n");
			return -EFAULT;
		}

		ret = sunxi_tsc_unmap_ionphys(&user_ion_param, 0);
		if (ret) {
			TSC_MSG("TSCDEV_RELEASE_IONPHYADDR unmap_ionphys error\n");
			return -EINVAL;
		}

		TSC_MSG("TSCDEV_RELEASE_IONPHYADDR free success fd=%d\n", user_ion_param.fd);
		break;
	}

	default:
		TSC_MSG("invalid cmd!\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int tscdev_open(struct inode *inode, struct file *filp)
{
	/* unsigned long clk_rate; */
	if (down_interruptible(&tsc_devp->sem)) {
		TSC_MSG("down interruptible failed!\n");
		return -ERESTARTSYS;
	}
	/* init other resource here */
	tsc_devp->irq_flag = 0;
	up(&tsc_devp->sem);
	nonseekable_open(inode, filp);
	return 0;
}

static int tscdev_release(struct inode *inode, struct file *filp)
{
	if (down_interruptible(&tsc_devp->sem))
		return -ERESTARTSYS;
	sunxi_tsc_close_all_filters(tsc_devp);
	/* release other resource here */
	tsc_devp->irq_flag = 1;
	sunxi_tsc_unmap_ionphys(NULL, 1);
	up(&tsc_devp->sem);
	return 0;
}

void tscdev_vma_open(struct vm_area_struct *vma)
{
	TSC_INFO();
}

void tscdev_vma_close(struct vm_area_struct *vma)
{
	TSC_INFO();
}

static struct vm_operations_struct tscdev_remap_vm_ops = {
	.open  = tscdev_vma_open,
	.close = tscdev_vma_close,
};

static int tscdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long temp_pfn;

	if (vma->vm_end - vma->vm_start == 0) {
		TSC_MSG("vm_end is equal vm_start : %lx\n", vma->vm_start);
		return 0;
	}
	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT)) {
		TSC_MSG(
		"vm_pgoff is %lx,it is large than the largest page number\n",
			vma->vm_pgoff);
		return -EINVAL;
	}
	temp_pfn = tsc_devp->mapbase >> 12;
	/* Set reserved and I/O flag for the area. */
	vma->vm_flags |= /*VM_RESERVED | */VM_IO;
	/* Select uncached access. */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (io_remap_pfn_range(vma, vma->vm_start, temp_pfn,
		(vma->vm_end - vma->vm_start), vma->vm_page_prot)) {
		return -EAGAIN;
	}
	vma->vm_ops = &tscdev_remap_vm_ops;
	tscdev_vma_open(vma);
	return 0;
}


static struct file_operations tscdev_fops = {
	.owner          = THIS_MODULE,
	.mmap           = tscdev_mmap,
	.poll           = tscdev_poll,
	.open           = tscdev_open,
	.release        = tscdev_release,
	.llseek         = no_llseek,
	.unlocked_ioctl = tscdev_ioctl,
};

static int tscdev_init(struct platform_device *pdev)
{
	int ret = 0;
	int devno;
	struct device_node *node;
	struct resource *mem_res;
	dev_t dev;

	dev = 0;
	node = pdev->dev.of_node;

	tsc_devp = kmalloc(sizeof(struct tsc_dev), GFP_KERNEL);
	if (tsc_devp == NULL) {
		TSC_MSG("malloc mem for tsc device err\n");
		ret = -ENOMEM;
		goto error0;
	}
	memset(tsc_devp, 0, sizeof(struct tsc_dev));

	tsc_devp->platform_dev = &pdev->dev;

	/* register or alloc the device number. */
	tsc_devp->major = TSCDEV_MAJOR;
	tsc_devp->minor = TSCDEV_MINOR;
	if (tsc_devp->major) {
		dev = MKDEV(tsc_devp->major, tsc_devp->minor);
		ret = register_chrdev_region(dev, 1, "ts0");
	} else {
		ret = alloc_chrdev_region(&dev, tsc_devp->minor, 1, "ts0");
		tsc_devp->major = MAJOR(dev);
		tsc_devp->minor = MINOR(dev);
	}
	if (ret < 0) {
		TSC_MSG("ts0: can't get major: %d.\n", tsc_devp->major);
		ret = -EINVAL;
		goto error0;
	}
	spin_lock_init(&tsc_devp->lock);
	mutex_init(&tsc_devp->lock_mem);
	AW_MEM_INIT_LIST_HEAD(&tsc_devp->list);

	printk("[tsc] %s() - %d \n", __func__, __LINE__);
	/* get physical address */
	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem_res == NULL) {
		TSC_MSG("get memory resource failed\n");
		ret = -ENOMEM;
		goto error0;
	}
	tsc_devp->mapbase = mem_res->start;

	tsc_devp->irq = irq_of_parse_and_map(node, 0);
	if (tsc_devp->irq <= 0) {
		TSC_MSG("can not parse irq.\n");
		ret = -EINVAL;
		goto error0;
	}

	printk("[tsc] %s() - %d \n", __func__, __LINE__);
	sema_init(&tsc_devp->sem, 1);
	init_waitqueue_head(&tsc_devp->wq);
	memset(&tsc_devp->iomap_addrs, 0, sizeof(struct iomap_para));
	ret = request_irq(tsc_devp->irq, sunxi_tsc_irq_handle, 0, "ts0", NULL);
	if (ret < 0) {
		TSC_MSG("request irq err\n");
		ret = -EINVAL;
		goto error0;
	}
	/* map for macc io space */
	tsc_devp->iomap_addrs.regs_macc = of_iomap(node, 0);
	if (!tsc_devp->iomap_addrs.regs_macc) {
		TSC_MSG("tsc can't map registers.\n");
		ret = -EINVAL;
		goto error0;
	}
	/* init tsc hw clk */
	sunxi_tsc_hw_clk_init(pdev);
	/* Create char device */
	devno = MKDEV(tsc_devp->major, tsc_devp->minor);
	cdev_init(&tsc_devp->cdev, &tscdev_fops);
	tsc_devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&tsc_devp->cdev, devno, 1);
	if (ret) {
		TSC_MSG("err:%d add tscdev.", ret);
		ret = -EINVAL;
		goto error0;
	}
	tsc_devp->tsc_class = class_create(THIS_MODULE, "ts0");
	tsc_devp->dev = device_create(tsc_devp->tsc_class, NULL, devno, NULL, "ts0");

	if (sunxi_tsc_get_port_config(pdev) < 0) {
		TSC_ERR("get tsc port config failed!\n");
		ret = -EINVAL;
		goto error0;
	}

	if (sunxi_tsc_request_gpio(pdev)) {
		TSC_ERR("request tsc pio failed!\n");
		ret = -EINVAL;
		goto error0;
	}

	printk("[tsc] %s() - %d \n", __func__, __LINE__);
	TSC_MSG("init succussful\n");
	return 0;

error0:
	if (tsc_devp) {
		kfree(tsc_devp);
		tsc_devp = NULL;
	}
	return ret;
}

static void tscdev_exit(void)
{
	dev_t dev;
	int ret = 0;

	dev = MKDEV(tsc_devp->major, tsc_devp->minor);
	free_irq(tsc_devp->irq, NULL);
	iounmap(tsc_devp->iomap_addrs.regs_macc);

    /* Destroy char device */
	if (tsc_devp) {
		cdev_del(&tsc_devp->cdev);
		device_destroy(tsc_devp->tsc_class, dev);
		class_destroy(tsc_devp->tsc_class);
	}

	if (NULL == tsc_devp->mclk || IS_ERR(tsc_devp->mclk)) {
		TSC_MSG("mclk handle is invalid.\n");
	} else {
		/* clk_disable_unprepare(tsc_devp->mclk); */
		ret = sunxi_tsc_disable_hw_clk();
		if (ret < 0) {
			TSC_ERR("tsc clk disable failed.\n");
		}
		clk_put(tsc_devp->mclk);
		tsc_devp->mclk = NULL;
	}
	if (NULL == tsc_devp->pclk || IS_ERR(tsc_devp->pclk)) {
		TSC_MSG("parent pll clk handle is invalid.\n");
	} else {
		clk_put(tsc_devp->pclk);
	}
	/* release ts pin */
	sunxi_tsc_disable_gpio();
	sunxi_tsc_release_gpio();
	unregister_chrdev_region(dev, 1);
	if (tsc_devp) {
		kfree(tsc_devp);
		tsc_devp = NULL;
	}
}


#ifdef CONFIG_PM
static int sw_tsc_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;

	ret = sunxi_tsc_disable_gpio();
	if (ret < 0) {
		TSC_MSG("tsc release gpio failed!\n");
		return -EFAULT;
	}
	ret = sunxi_tsc_disable_hw_clk();
	if (ret < 0) {
		TSC_ERR("tsc clk disable failed!\n");
		return -EFAULT;
	}
	TSC_MSG("standby suspend succeed!\n");
	return 0;
}
static int sw_tsc_resume(struct platform_device *pdev)
{
	int ret = 0;

	ret = sunxi_tsc_enable_hw_clk();
	if (ret < 0) {
		TSC_ERR("tsc clk enable failed!\n");
		return -EFAULT;
	}
	ret = sunxi_tsc_request_gpio(pdev);
	if (ret < 0) {
		TSC_ERR("request tsc pio failed!\n");
		return -EFAULT;
	}
	TSC_MSG("standby resume succeed!\n");
	return 0;
}
#endif

static int sunxi_tsc_remove(struct platform_device *pdev)
{
	tscdev_exit();
	return 0;
}

static int sunxi_tsc_probe(struct platform_device *pdev)
{
	printk("[tsc] %s() - %d \n", __func__, __LINE__);
	tscdev_init(pdev);
	return 0;
}

static struct platform_driver sunxi_tsc_driver = {
	.probe = sunxi_tsc_probe,
	.remove = sunxi_tsc_remove,
#ifdef CONFIG_PM
	.suspend = sw_tsc_suspend,
	.resume  = sw_tsc_resume,
#endif
	.driver  = {
		.name = "ts0",
		.owner = THIS_MODULE,
		.of_match_table = sunxi_tsc_match,
	},
};

static int __init sunxi_tsc_init(void)

{
	int ret;
	ret = platform_driver_register(&sunxi_tsc_driver);
	return ret;
}

static void __exit sunxi_tsc_exit(void)
{
	platform_driver_unregister(&sunxi_tsc_driver);
}

module_init(sunxi_tsc_init);
module_exit(sunxi_tsc_exit);
MODULE_AUTHOR("Soft-Reuuimlla");
MODULE_DESCRIPTION("User mode tsc device interface");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:tsc-sunxi");
