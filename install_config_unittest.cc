// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "chromeos_install_config.h"
#include "chromeos_postinst.h"
#include "inst_util.h"

using std::string;

void TestConfigureInstall(const std::string& install_dev,
                          const std::string& install_path,
                          bool expected_success,
                          const std::string& expected_slot,
                          const std::string& expected_root,
                          const std::string& expected_boot) {

  InstallConfig install_config;

  EXPECT_EQ(ConfigureInstall(install_dev,
                             install_path,
                             &install_config),
            expected_success);

  if (!expected_success)
    return;

  EXPECT_EQ(install_config.slot, expected_slot);
  EXPECT_EQ(install_config.root.device(), expected_root);
  EXPECT_EQ(install_config.boot.device(), expected_boot);
}

class InstallConfigTest : public ::testing::Test { };

TEST(InstallConfigTest, ConfigureInstallTest) {
  TestConfigureInstall("/dev/sda3", "/mnt",
                       true,
                       "A", "/dev/sda3", "/dev/sda1");
  TestConfigureInstall("/dev/sda4", "/mnt",
                       true,
                       "B", "/dev/sda4", "/dev/sda1");
  TestConfigureInstall("/dev/mmcblk0p3", "/mnt",
                       true, "A",
                       "/dev/mmcblk0p3", "/dev/mmcblk0p1");
  TestConfigureInstall("/dev/mmcblk0p4", "/mnt",
                       true, "B",
                       "/dev/mmcblk0p4", "/dev/mmcblk0p1");
  TestConfigureInstall("/dev/sda2", "/mnt",
                       false, "", "", "");
  TestConfigureInstall("/dev/sda", "/mnt",
                       false, "", "", "");
}
