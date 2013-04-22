// Copyright (c) 2013 The CoreOS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "coreos_legacy.h"

#include <stdio.h>
#include <unistd.h>

#include "inst_util.h"

using std::string;

bool RunPyGrubPostInstall(const InstallConfig& install_config) {
  printf("Running LegacyPostInstall\n");

  string cmd = StringPrintf("cp -nR '%s/boot/syslinux' '%s'",
                            install_config.root.mount().c_str(),
                            install_config.boot.mount().c_str());
  if (RunCommand(cmd.c_str()) != 0) {
    printf("Cmd: '%s' failed.\n", cmd.c_str());
    return false;
  }

  return true;
}
