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

The repository agent for verifying file checksum before loading the model.
The repository agent comunicates with Triton using [Triton RepoAgent API](https://github.com/triton-inference-server/core/tree/main/include/triton/core/tritonrepoagent.h).
Ask questions or report problems in the main Triton [issues
page](https://github.com/triton-inference-server/server/issues).

## Frequently Asked Questions

Full documentation is included below but these shortcuts can help you
get started in the right direction.

### Where can I ask general questions about Triton and Triton backends?

Be sure to read all the information below as well as the [general
Triton
documentation](https://github.com/triton-inference-server/server#triton-inference-server)
available in the main
[server](https://github.com/triton-inference-server/server) repo. If
you don't find your answer there you can ask questions on the main
Triton [issues
page](https://github.com/triton-inference-server/server/issues).

### How do I build the checksum repository agent?

See [build instructions](#build-the-checksum-repository-agent) below.

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
but the listed CMake argument can be used to override.

* triton-inference-server/core: -DTRITON_CORE_REPO_TAG=[tag]
* triton-inference-server/common: -DTRITON_COMMON_REPO_TAG=[tag]

## Set Up the Checksum Repository Agent

Each repository agent must be implemented as a shared library and the name of
the shared library must be *libtritonrepoagent_<repo-agent-name>.so*. For
checksum repository agent, if the name of it is "checksum", a model indicates
that it uses the repository agent by setting "checksum" as 'name' in agent setting
inside the model configuration, and Triton looks for
*libtritonrepoagent_checksum.so* as the shared library that implements
the checksum repository agent.

For a model, *M* that specifies repository agent *A*, Triton searches for the
repository agent shared library in the following places, in this order:

* <model_repository>/M/libtritonrepoagent__B.so

* <repository_agent_directory>/libtritonrepoagent__B.so

Where <repository_agent_directory> is by default /opt/tritonserver/repoagents.

The repository agent can be configured by specifying 'parameters' in agent
setting inside the model configuration. The checksum repository agent will
examine the parameters in the following way: the key specifies the message
digest algorithm that is used to generate the checksum and the path to file
relative to the model repository as <algorithm:relative/path/to/file> and the
value specifies the expected checksum of the file. For example, if a model uses
checksum repository agent to verify a file inside the model repository named
"embedding_table", given the expected MD5 checksum of the file to be
d726e132f91f16ebee703d96f6f73cb1, the model configuration will be:

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

With the above setup, the checksum repository agent will be invoked before
loading the model and reject the loading if the checksum does not match.
