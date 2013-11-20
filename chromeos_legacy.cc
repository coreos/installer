// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_legacy.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "inst_util.h"

#include "CgptManager.h"

using std::string;

bool RunLegacyBootloaderInstall(const InstallConfig& install_config) {
  printf("Running LegacyPostInstall\n");

  // Copy the correct menu.lst into place for cloud bootloaders that want
  // a /boot/grub/menu.lst file
  string menu_from = StringPrintf("%s/boot/grub/menu.lst.%s",
                                    install_config.root.mount().c_str(),
                                    install_config.slot.c_str());

  string menu_to = StringPrintf("%s/boot/grub/menu.lst",
                                    install_config.boot.mount().c_str());

  if (!CopyFile(menu_from, menu_to))
    return false;

  string kernel_from = StringPrintf("%s/boot/vmlinuz",
                                    install_config.root.mount().c_str());

  string kernel_to = StringPrintf("%s/syslinux/vmlinuz.%s",
                                  install_config.boot.mount().c_str(),
                                  install_config.slot.c_str());

  if (!CopyFile(kernel_from, kernel_to))
    return false;

  // Copy the correct root.A/B.cfg for syslinux
  string root_cfg_from = StringPrintf("%s/boot/syslinux/root.%s.cfg",
                                      install_config.root.mount().c_str(),
                                      install_config.slot.c_str());

  string root_cfg_to = StringPrintf("%s/syslinux/root.%s.cfg",
                                    install_config.boot.mount().c_str(),
                                    install_config.slot.c_str());

  if (!CopyFile(root_cfg_from, root_cfg_to))
    return false;

  return true;
}


bool RunCgptInstall(const InstallConfig& install_config) {
  printf("Updating Partition Table Attributes using CgptManager...\n");

  CgptManager cgpt_manager;

  int result = cgpt_manager.Initialize(install_config.root.base_device());
  if (result != kCgptSuccess) {
    printf("Unable to initialize CgptManager\n");
    return false;
  }

  result = cgpt_manager.SetHighestPriority(install_config.root.number());
  if (result != kCgptSuccess) {
    printf("Unable to set highest priority for root %d\n",
           install_config.root.number());
    return false;
  }

  int numTries = 1;
  result = cgpt_manager.SetNumTriesLeft(install_config.root.number(),
                                        numTries);
  if (result != kCgptSuccess) {
    printf("Unable to set NumTriesLeft to %d for root %d\n",
           numTries,
           install_config.root.number());
    return false;
  }

  printf("Updated root %d with highest priority and NumTriesLeft = %d\n",
         install_config.root.number(), numTries);

  return true;
}

bool RunLegacyPostInstall(const InstallConfig& install_config) {
  if (!RunLegacyBootloaderInstall(install_config)) {
    return false;
  }

  return RunCgptInstall(install_config);
}
