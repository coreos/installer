// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_postinst.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "CgptManager.h"

#include "chromeos_legacy.h"
#include "chromeos_install_config.h"
#include "chromeos_setimage.h"
#include "inst_util.h"

using std::string;

bool ConfigureInstall(
    const std::string& install_dev,
    const std::string& install_path,
    InstallConfig* install_config) {

  Partition root = Partition(install_dev, install_path);

  string slot;
  switch (root.number()) {
    case 3:
      slot = "A";
      break;
    case 4:
      slot = "B";
      break;
    default:
      fprintf(stderr,
              "Not a valid target parition number: %i\n", root.number());
      return false;
  }

  string boot_dev = MakePartitionDev(root.base_device(), 1);

  // Put the actual values on the result structure
  install_config->slot = slot;
  install_config->root = root;
  install_config->boot = Partition(boot_dev);

  return true;
}

// Updates firmware. We must activate new firmware only after new kernel is
// actived (installed and made bootable), otherwise new firmware with all old
// kernels may lead to recovery screen (due to new key).
// TODO(hungte) Replace the shell execution by native code (crosbug.com/25407).
bool FirmwareUpdate(const string &install_dir, bool is_update) {
  int result;
  const char *mode;
  string command = install_dir + "/usr/sbin/chromeos-firmwareupdate";

  if (access(command.c_str(), X_OK) != 0) {
    printf("No firmware updates available.\n");
    return true;
  }

  if (is_update) {
    // Background auto update by Update Engine.
    mode = "autoupdate";
  } else {
    // Recovery image, or from command "chromeos-install".
    mode = "recovery";
  }

  command += " --mode=";
  command += mode;

  printf("Starting firmware updater (%s)\n", command.c_str());
  result = system(command.c_str());

  // Next step after postinst may take a lot of time (eg, disk wiping)
  // and people may confuse that as 'firmware update takes a long wait',
  // we explicitly prompt here.
  if (result == 0) {
    printf("Firmware update completed.\n");
  } else if (result == 3) {
    printf("Firmware can't be updated because booted from B (error code: %d)\n",
           result);
  } else {
    printf("Firmware update failed (error code: %d).\n", result);
  }

  return result == 0;
}

// Matches commandline arguments of chrome-chroot-postinst
//
// src_version of the form "10.2.3.4" or "12.3.2"
// install_dev of the form "/dev/sda3"
//
bool ChromeosChrootPostinst(const InstallConfig& install_config,
                            string src_version) {

  printf("ChromeosChrootPostinst(%s)\n",
         src_version.c_str());

  // Extract External ENVs
  bool is_factory_install = getenv("IS_FACTORY_INSTALL");
  bool is_recovery_install = getenv("IS_RECOVERY_INSTALL");
  bool is_install = getenv("IS_INSTALL");
  bool is_update = !is_factory_install && !is_recovery_install && !is_install;

  bool make_dev_readonly = false;

  if (is_update && VersionLess(src_version, "0.10.156.2")) {
    // See bug chromium-os:11517. This fixes an old FS corruption problem.
    printf("Patching new rootfs\n");
    if (!R10FileSystemPatch(install_config.root.device()))
      return false;
    make_dev_readonly=true;
  }

  // If this FS was mounted read-write, we can't do deltas from it. Mark the
  // FS as such
  Touch(install_config.root.mount() + "/.nodelta");  // Ignore Error on purpse

  printf("Set boot target to %s: Partition %d, Slot %s\n",
         install_config.root.device().c_str(),
         install_config.root.number(),
         install_config.slot.c_str());

  if (!SetImage(install_config)) {
    printf("SetImage failed.\n");
    return false;
  }

  // This cache file might be invalidated, and will be recreated on next boot.
  // Error ignored, since we don't care if it didn't exist to start with.
  string network_driver_cache = "/var/lib/preload-network-drivers";
  printf("Clearing network driver boot cache: %s.\n",
         network_driver_cache.c_str());
  unlink(network_driver_cache.c_str());

  printf("Syncing filesystems before changing boot order...\n");
  sync();

  if (make_dev_readonly) {
    printf("Making dev %s read-only\n", install_config.root.device().c_str());
    MakeDeviceReadOnly(install_config.root.device());  // Ignore error
  }

  // At this point in the script, the new partition has been marked bootable
  // and a reboot will boot into it. Thus, it's important that any future
  // errors in this script do not cause this script to return failure unless
  // in factory mode.

  // We have a new image, making the ureadahead pack files
  // out-of-date.  Delete the files so that ureadahead will
  // regenerate them on the next reboot.
  // WARNING: This doesn't work with upgrade from USB, rather than full
  // install/recovery. We don't have support for it as it'll increase the
  // complexity here, and only developers do upgrade from USB.
  if (!RemovePackFiles("/var/lib/ureadahead")) {
    printf("RemovePackFiles Failed\n");
  }

  // Create a file indicating that the install is completed. The file
  // will be used in /sbin/chromeos_startup to run tasks on the next boot.
  // See comments above about removing ureadahead files.
  if (!Touch("/media/state/.install_completed")) {
    printf("Touch(/media/state/.install_completed) FAILED\n");
    if (is_factory_install)
      return false;
  }

  printf("ChromeosChrootPostinst complete\n");
  return true;
}

bool RunPostInstall(const string& install_dir,
                    const string& install_dev) {
  InstallConfig install_config;

  if (!ConfigureInstall(install_dir,
                        install_dev,
                        &install_config)) {
    printf("Configure failed.\n");
    return false;
  }

  // Log how we are configured.
  printf("PostInstall Configured: (%s, %s, %s)\n",
         install_config.slot.c_str(),
         install_config.root.device().c_str(),
         install_config.boot.device().c_str());

  // If we can read in the lsb-release we are updating FROM, log it.
  string lsb_contents;
  if (ReadFileToString("/etc/lsb-release", &lsb_contents)) {
    printf("\nFROM (rootfs):\n%s", lsb_contents.c_str());
  }

  // If we can read in the stateful lsb-release we are updating FROM, log it.
  if (ReadFileToString("/media/state/etc/lsb-release",
                       &lsb_contents)) {
    printf("\nFROM (stateful):\n%s", lsb_contents.c_str());
  }

  // If we can read the lsb-release we are updating TO, log it
  if (ReadFileToString(install_config.root.mount() + "/etc/lsb-release",
                       &lsb_contents)) {
    printf("\nTO:\n%s\n", lsb_contents.c_str());
  }


  string src_version;
  if (!LsbReleaseValue("/etc/lsb-release",
                       "COREOS_RELEASE_VERSION",
                       &src_version) ||
      src_version.empty()) {
    printf("Failed to read /etc/lsb-release\n");
    return false;
  }

  if (!ChromeosChrootPostinst(install_config, src_version)) {
    printf("PostInstall Failed\n");
    return false;
  }

  printf("Syncing filesystem at end of postinst...\n");
  sync();

  // Sync doesn't appear to sync out cgpt changes, so
  // let them flush themselves. (chromium-os:35992)
  sleep(10);

  install_config.boot.set_mount("/tmp/boot_mnt");

  string cmd;

  cmd = StringPrintf("/bin/mkdir -p %s",
                     install_config.boot.mount().c_str());
  RUN_OR_RETURN_FALSE(cmd);

  cmd = StringPrintf("/bin/mount %s %s",
                     install_config.boot.device().c_str(),
                     install_config.boot.mount().c_str());
  RUN_OR_RETURN_FALSE(cmd);

  bool success = true;

  if (!RunLegacyPostInstall(install_config)) {
    printf("Legacy PostInstall failed.\n");
    success = false;
  }

  cmd = StringPrintf("/bin/umount %s",
                     install_config.boot.device().c_str());
  if (RunCommand(cmd.c_str()) != 0) {
    printf("Cmd: '%s' failed.\n", cmd.c_str());
    success = false;
  }

  return success;
}
