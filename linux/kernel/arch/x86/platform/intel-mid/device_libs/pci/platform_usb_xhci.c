
/* platform_usb_xhci.c: USB XHCI platform quirk file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <asm/intel-mid.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

static void __devinit xhci_pci_early_quirks(struct pci_dev *pci_dev)
{
	dev_dbg(&pci_dev->dev, "set run wake flag\n");
	device_set_run_wake(&pci_dev->dev, true);
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BYT_USH,
			xhci_pci_early_quirks);

static void quirk_byt_ush_d3_delay(struct pci_dev *dev)
{
	dev->d3_delay = 10;
}
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BYT_USH,
			quirk_byt_ush_d3_delay);

#define PCI_USH_SSCFG1		0xb0
#define PCI_USH_SSCFG1_D3	BIT(28)
#define PCI_USH_SSCFG1_SUS	BIT(30)

#define PCI_USH_OP_OFFSET	0x80
#define PCI_USH_OP_PORTSC_OFFSET	0x400
#define PCI_USH_OP_PORTSC_CCS	BIT(0)
#define PCI_USH_MAX_PORTS	4

static void quirk_byt_ush_suspend(struct pci_dev *dev)
{
	struct usb_hcd	*hcd;
	u32	portsc;
	u32	value;
	int	port_index = 0;
	int	usb_attached = 0;

	dev_dbg(&dev->dev, "USH suspend quirk\n");

	hcd = pci_get_drvdata(dev);
	if (!hcd)
		return;

	/* Check if anything attached on USB ports,
	 * FIXME: may need to check HSIC ports */
	while (port_index < PCI_USH_MAX_PORTS) {
		portsc = readl(hcd->regs + PCI_USH_OP_OFFSET +
				PCI_USH_OP_PORTSC_OFFSET +
				port_index * 0x10);
		if (portsc & PCI_USH_OP_PORTSC_CCS) {
			pr_info("XHCI: port %d, portsc 0x%x\n",
				port_index, portsc);
			usb_attached = 1;
			break;
		}
		port_index++;
	}

	pci_read_config_dword(dev, PCI_USH_SSCFG1, &value);

	/* set SSCFG1 BIT 28 and 30 before enter D3hot
	 * if USB attached, then we can not turn off SUS */
	if (usb_attached)
		value |= PCI_USH_SSCFG1_D3;
	else
		value |= (PCI_USH_SSCFG1_D3 | PCI_USH_SSCFG1_SUS);
	pci_write_config_dword(dev, PCI_USH_SSCFG1, value);

	pci_read_config_dword(dev, PCI_USH_SSCFG1, &value);
	dev_dbg(&dev->dev, "PCI B0 reg (SSCFG1) = 0x%x\n", value);
}
DECLARE_PCI_FIXUP_SUSPEND(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BYT_USH,
			quirk_byt_ush_suspend);

static void quirk_byt_ush_resume(struct pci_dev *dev)
{
	u32	value;

	dev_dbg(&dev->dev, "USH resume quirk\n");
	pci_read_config_dword(dev, PCI_USH_SSCFG1, &value);

	/* clear SSCFG1 BIT 28 and 30 after back to D0 */
	value &= (~(PCI_USH_SSCFG1_D3 | PCI_USH_SSCFG1_SUS));
	pci_write_config_dword(dev, PCI_USH_SSCFG1, value);

	pci_read_config_dword(dev, PCI_USH_SSCFG1, &value);
	dev_dbg(&dev->dev, "PCI B0 reg (SSCFG1) = 0x%x\n", value);
}
DECLARE_PCI_FIXUP_RESUME(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BYT_USH,
			quirk_byt_ush_resume);
