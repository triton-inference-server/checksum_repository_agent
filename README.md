<!--
# Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-->

[![License](https://img.shields.io/badge/License-BSD3-lightgrey.svg)](https://opensource.org/licenses/BSD-3-Clause)

# Triton Checksum Repository Agent

This repo contains an example [repository
agent](https://github.com/triton-inference-server/server/blob/master/docs/repository_agents.md)
for verifying file checksums before loading the model.  Ask questions
or report problems in the main Triton [issues
page](https://github.com/triton-inference-server/server/issues).

## Build the Checksum Repository Agent

Use a recent cmake to build. First install the required dependencies.

```
$ apt-get install openssl-dev
```

To build the repository agent:

```
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/install ..
$ make install
```

The following required Triton repositories will be pulled and used in
the build. By default the "main" branch/tag will be used for each repo
but the following CMake arguments can be used to override.

* triton-inference-server/core: -DTRITON_CORE_REPO_TAG=[tag]
* triton-inference-server/common: -DTRITON_COMMON_REPO_TAG=[tag]

## Using the Checksum Repository Agent

The checksum repository agent is configured by specifying expected
checksum values in the [*ModelRepositoryAgents* section of the model
configuration](https://github.com/triton-inference-server/common/blob/main/protobuf/model_config.proto). A
separate parameter is used for each file checksum in the following
way: the key specifies the message digest algorithm that is used to
generate the checksum and the path to file relative to the model
repository as \<algorithm:relative/path/to/file\> and the value
specifies the expected checksum of the file. Currently the checksum
repository agent only supports MD5 checksums. For example:

```
model_repository_agents
{
  agents [
    {
      name: "checksum",
      parameters
      {
        key: "MD5:embedding_table",
        value: "d726e132f91f16ebee703d96f6f73cb1"
      }
    }
  ]
}
```

With the above configuration, the checksum repository agent will be
invoked before loading the model and loading will fail if the MD5
checksum for file "embedding_table" does not match the specified
value.
