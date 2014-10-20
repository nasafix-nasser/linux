LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifneq ($(TARGET_SIMULATOR),true)
KERNEL_DEFCONFIG := $(LOCAL_PATH)/arch/$(TARGET_ARCH)/configs/realv210_defconfig
KERNEL_OUTDIR := $(TARGET_OUT_INTERMEDIATES)/KERNEL

TARGET_KERNEL_SRCDIR := $(LOCAL_PATH)
TARGET_KERNEL_IMAGE := $(KERNEL_OUTDIR)/arch/$(TARGET_ARCH)/boot/zImage
INSTALLED_TARGET_KERNEL_IMAGE := $(PRODUCT_OUT)/kernel.img
TARGET_KERNEL_CONFIG := $(KERNEL_OUTDIR)/.config
TARGET_KERNEL_MODULES := $(KERNEL_OUTDIR)/arch/$(TARGET_ARCH)/modules/
#TARGET_WLAN_DRIVER := $(TARGET_KERNEL_MODULES)/lib/modules/2.6.29-rc3-mokodev/kernel/drivers/ar6000/ar6000.ko
#INSTALLED_WLAN_DRIVER := $(PRODUCT_OUT)/system/lib/modules/ar6000.ko

ROOT_DIR := $(abspath $(TARGET_ROOT_OUT))
ROOT_UID := $(shell id -u)
ROOT_GID := $(shell id -g)

define kernel-make
	$(MAKE) -s --no-print-directory \
		-C $(TARGET_KERNEL_SRCDIR) \
		-f Makefile \
		CROSS_COMPILE=$(abspath $(TARGET_TOOLS_PREFIX)) \
		O=$(abspath $(KERNEL_OUTDIR)) \
		ARCH=$(TARGET_ARCH)
endef

$(TARGET_KERNEL_IMAGE): $(TARGET_KERNEL_CONFIG) $(PRODUCT_OUT)/ramdisk.img
	$(hide) $(kernel-make) zImage modules modules_install INSTALL_MOD_PATH=$(abspath $(TARGET_KERNEL_MODULES))

$(TARGET_KERNEL_CONFIG): $(KERNEL_DEFCONFIG)
	$(hide) mkdir -p $(@D)
	$(hide) perl -p \
		-e 's{###ROOT_DIR###}{$(ROOT_DIR)};' \
		-e 's{###ROOT_UID###}{$(ROOT_UID)};' \
		-e 's{###ROOT_GID###}{$(ROOT_GID)};' \
		$(KERNEL_DEFCONFIG) > $@

$(INSTALLED_TARGET_KERNEL_IMAGE): $(TARGET_KERNEL_IMAGE)
	$(hide) echo Install kernel image: $@
	$(hide) $(copy-file-to-target)

#$(INSTALLED_WLAN_DRIVER): $(TARGET_WLAN_DRIVER)
#	$(hide) echo Install WLAN driver: $@
#	$(hide) $(copy-file-to-target)

CUSTOM_MODULES += $(INSTALLED_TARGET_KERNEL_IMAGE)

#CUSTOM_MODULES += $(INSTALLED_WLAN_DRIVER)

kernel-clean:
	$(hide) $(RM) -rf $(KERNEL_OUTDIR)
	$(hide) $(RM) $(INSTALLED_TARGET_KERNEL_IMAGE)
#	$(hide) $(RM) $(INSTALLED_WLAN_DRIVER)

kernel-image: $(TARGET_KERNEL_CONFIG) $(PRODUCT_OUT)/ramdisk.img
	$(hide) $(kernel-make) zImage

kernel-%:
	$(hide) $(kernel-make) $*

.PHONY: kernel-clean kernel-menuconfig kernel-image

endif
