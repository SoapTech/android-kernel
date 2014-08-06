
#ifndef __INTEL_CRYSTALCOVE_PWRSRC_H_
#define __INTEL_CRYSTALCOVE_PWRSRC_H_

int crystal_cove_enable_vbus(void);
int crystal_cove_disable_vbus(void);
int crystal_cove_unmask_vbus_intr(void);
int crystal_cove_mask_vbus_intr(void);
#endif
