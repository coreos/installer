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

  // Of the form: PARTUUID=XXX-YYY-ZZZ
  string root_uuid = StringPrintf("PARTUUID=%s",
                                  install_config.root.uuid().c_str());

  string verity_enabled = "coreos";
  string default_syslinux_cfg = StringPrintf("DEFAULT %s.%s\n",
                                             verity_enabled.c_str(),
                                             install_config.slot.c_str());

  if (!WriteStringToFile(default_syslinux_cfg,
                         StringPrintf("%s/syslinux/default.cfg",
                                      install_config.boot.mount().c_str())))
    return false;

  // Prepare the new root.A/B.cfg

  string root_cfg_file = StringPrintf("%s/syslinux/root.%s.cfg",
                                      install_config.boot.mount().c_str(),
                                      install_config.slot.c_str());

  // Copy over the unmodified version for this release...
  if (!CopyFile(StringPrintf("%s/boot/syslinux/root.%s.cfg",
                             install_config.root.mount().c_str(),
                             install_config.slot.c_str()),
                root_cfg_file))
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
    printf("Unable to set highest priority for kernel %d\n",
           install_config.kernel.number());
    return false;
  }

  // Extract External ENVs
  bool is_factory_install = getenv("IS_FACTORY_INSTALL");
  bool is_recovery_install = getenv("IS_RECOVERY_INSTALL");
  bool is_install = getenv("IS_INSTALL");
  bool is_update = !is_factory_install && !is_recovery_install && !is_install;

  // If it's not an update, pre-mark the first boot as successful
  // since we can't fall back on the old install.
  bool new_root_successful = !is_update;
  result = cgpt_manager.SetSuccessful(install_config.root.number(),
                                      new_root_successful);
  if (result != kCgptSuccess) {
    printf("Unable to set successful to %d for kernel %d\n",
           new_root_successful,
           install_config.kernel.number());
    return false;
  }

  int numTries = 1;
  result = cgpt_manager.SetNumTriesLeft(install_config.root.number(),
                                        numTries);
  if (result != kCgptSuccess) {
    printf("Unable to set NumTriesLeft to %d for kernel %d\n",
           numTries,
           install_config.kernel.number());
    return false;
  }

  printf("Updated root %d with Successful = %d and NumTriesLeft = %d\n",
         install_config.root.number(), new_root_successful, numTries);


  return true;
}

bool RunLegacyPostInstall(const InstallConfig& install_config) {
  if (!RunLegacyBootloaderInstall(install_config)) {
    return false;
  }

  return RunCgptInstall(install_config);
}

bool RunLegacyUBootPostInstall(const InstallConfig& install_config) {
  printf("Running LegacyUBootPostInstall\n");

  string src_img = StringPrintf("%s/boot/boot-%s.scr.uimg",
                                install_config.root.mount().c_str(),
                                install_config.slot.c_str());

  string dst_img = StringPrintf("%s/u-boot/boot.scr.uimg",
                                install_config.boot.mount().c_str());

  // If the source img file exists, copy it into place, else do
  // nothing.
  if (access(src_img.c_str(), R_OK) == 0) {
    printf("Copying '%s' to '%s'\n", src_img.c_str(), dst_img.c_str());
    return CopyFile(src_img, dst_img);
  } else {
    printf("Not present to install: '%s'\n", src_img.c_str());
    return true;
  }
}

bool RunEfiPostInstall(const InstallConfig& install_config) {
  printf("Running EfiPostInstall\n");
  printf("Editing grubpart%s PARTUUID to %s in %s using template in %s\n",
      install_config.slot.c_str(),
      install_config.root.uuid().c_str(),
      install_config.boot.mount().c_str(),
      install_config.root.mount().c_str());
  string grub_from = StringPrintf("%s/boot/efi/boot/grub.cfg",
                                  install_config.root.mount().c_str());
  string grub_to = StringPrintf("%s/efi/boot/grub.cfg",
                                install_config.boot.mount().c_str());

  // Find the line we want from the template.
  string grub_from_str;
  if (!ReadFileToString(grub_from, &grub_from_str)) {
    printf("Unable to read grub template file %s\n",
           grub_from.c_str());
    return false;
  }
  // Unfortunately std::regex is "experimental" at this time, so we have to
  // approximate the behavior of a regular-expression such as
  // "grubpartA.*linuxpartA".
  string s1 = StringPrintf("grubpart%s",
                           install_config.slot.c_str());
  string s2 = StringPrintf("linuxpart%s",
                           install_config.slot.c_str());
  string kernel_line;
  std::vector<string> file_lines;
  SplitString(grub_from_str, '\n', &file_lines);
  std::vector<string>::iterator line;
  for (line = file_lines.begin(); line < file_lines.end(); line++) {
    if ((line->find(s1) != string::npos) &&
        (line->find(s2) != string::npos)) {
      kernel_line = *line;
      break;
    }
  }
  if (kernel_line.empty()) {
    printf("error - bad grub.cfg template\n");
    return false;
  }

  // Substitute-in the correct UUID.
  string new_val = StringPrintf("PARTUUID=%s",
                                install_config.root.uuid().c_str());
  if (!SetKernelArg("root", new_val, &kernel_line)) {
    printf("error - setting new key value for root\n");
    return false;
  }

  // Overwrite the corresponding line in the actual boot menu.
  string new_boot_str;
  string grub_to_str;
  if (!ReadFileToString(grub_to, &grub_to_str)) {
    printf("Unable to read boot menu file %s\n",
           grub_to.c_str());
    return false;
  }
  // These search strings approximate the behavior of a regular-expression such
  // as "grubpartA.*PARTUUID".
  s1 = StringPrintf("grubpart%s",
                           install_config.slot.c_str());
  s2 = "PARTUUID";
  SplitString(grub_to_str, '\n', &file_lines);
  for (line = file_lines.begin(); line < file_lines.end(); line++) {
    if ((line->find(s1) != string::npos) &&
        (line->find(s2) != string::npos)) {
      new_boot_str += kernel_line;
      printf("Replaced:\n    %s\n  with:\n    %s\n",
             line->c_str(),
             kernel_line.c_str());
    } else {
      new_boot_str += *line;
    }
    new_boot_str += "\n";
  }
  if (!WriteStringToFile(new_boot_str, grub_to)) {
    printf("Unable to write boot menu file %s\n",
            grub_to.c_str());
    return false;
  }
  // Other EFI post-install actions, if any, go here.
  return true;
}
