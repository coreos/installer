// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_setimage.h"
#include "chromeos_verity.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "chromeos_install_config.h"
#include "inst_util.h"

using std::string;

//
// There is nothing in our codebase that calls chromeos-setimage,
// except for post install, so only it's use case is required
// here. I think.
//
// New dm argument syntax:
// TODO(taysom:defect 32847)
// In the future, the <num> field will be mandatory.
//
// <device>        ::= [<num>] <device-mapper>+
// <device-mapper> ::= <head> "," <target>+
// <head>          ::= <name> <uuid> <mode> [<num>]
// <target>        ::= <start> <length> <type> <options> ","
// <mode>          ::= "ro" | "rw"
// <uuid>          ::= xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx | "none"
// <type>          ::= "verity" | "bootcache" | ...
//
// Notes:
//  1. uuid is a label for the device and we set it to "none".
//  2. The <num> field will be optional initially and assumed to be 1.
//     Once all the scripts that set these fields have been set, it will
//     made mandatory.
//
// Coming attractions:
// The upstream version of verity does not use name/value pairs for
// arguments. All arguments are positional. The current code for
// finding the root_hexdigest and salt will need to change accordingly.
//
// Don't know if we can make the code parse both types of arguments.


bool SetImage(const InstallConfig& install_config) {

  printf("SetImage\n");

  // Re-hash the root filesystem and use the table for dm-verity.
  // We extract the parameters for verification from the kernel
  // partition, but we regenerate and reappend the hash tree to
  // keep the updater from needing to manage them explicitly.
  // Instead, rootfs integrity will be validated on next boot through
  // the verified kernel configuration.

  bool enable_rootfs_verification = 0;

  if (!enable_rootfs_verification)
    MakeFileSystemRw(install_config.root.device(), true);

  return true;
}
