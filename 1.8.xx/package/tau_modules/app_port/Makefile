#
# Copyright (C) 2006-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=portcount
PKG_RELEASE:=1

#PKG_BUILD_DIR := $(KERNEL_BUILD_DIR)/$(PKG_NAME)
PKG_BUILD_DIR:=$(TOPDIR)/local/tau_modules/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/portcount
  CATEGORY:=Local Packages
  SUBMENU:=TAU modules
  TITLE:=Update utility for trx firmware images
  DEPENDS:=+libreg +libpal
endef

define Package/portcount/description
 This package portcount app tool.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

target=$(firstword $(subst -, ,$(BOARD)))

MAKE_FLAGS += TARGET="$(target)"
TARGET_CFLAGS := -I$(LTE_INCLUDE) -I$(LINUX_DIR)/include $(TARGET_CFLAGS) -Dtarget_$(target)=1 -Wall


define Package/portcount/install

	if [ ! -d $(TARGET_DIR)/sbin/ ]; then \
		(mkdir -p $(TARGET_DIR)/sbin/ ); \
	fi

	$(INSTALL_DIR) $(1)/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/portcount $(1)/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/portcount $(TARGET_DIR)/sbin/


	
endef

$(eval $(call BuildPackage,portcount))
