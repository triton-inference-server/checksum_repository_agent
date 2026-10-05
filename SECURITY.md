<!--
# Copyright 2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

# Security Policy

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues,
discussions, or pull requests.**

NVIDIA takes the security of its software seriously. To report a potential
security vulnerability in this project, use one of the following channels:

1. **NVIDIA Vulnerability Disclosure Program (preferred):**
   [https://www.nvidia.com/en-us/security/](https://www.nvidia.com/en-us/security/)
2. **Email:** [psirt@nvidia.com](mailto:psirt@nvidia.com). Encrypt sensitive
   reports with NVIDIA's public PGP key:
   [https://www.nvidia.com/en-us/security/pgp-key](https://www.nvidia.com/en-us/security/pgp-key)
3. **GitHub Private Vulnerability Reporting:** use the "Report a vulnerability"
   button on the repository's **Security** tab, where enabled.

**OEM partners should contact their NVIDIA Customer Program Manager.**

Please include as much of the following as possible:

1. Product name and version, branch, or commit that contains the vulnerability
2. Type of vulnerability (for example path traversal, denial of service,
   integrity bypass)
3. Step-by-step instructions to reproduce the issue
4. Proof-of-concept or exploit code, if available
5. Potential impact, including how an attacker could exploit the issue

NVIDIA PSIRT acknowledges reports, assesses severity, coordinates a fix and
disclosure timeline with the reporter, and publishes security bulletins at
[https://www.nvidia.com/en-us/security/](https://www.nvidia.com/en-us/security/).

## Security Architecture and Context

**Project:** Triton Checksum Repository Agent, an example repository agent for
the [Triton Inference Server](https://github.com/triton-inference-server/server).
It is built as a shared library (`libtritonrepoagent_checksum.so`) that
implements the `TRITONREPOAGENT_ModelAction` entry point of the Triton
repository agent API (`src/checksum.cc`) and is loaded in-process by
`tritonserver`.

**Software classification:** Library (server plugin). It has no network
listener, no authentication layer, and no persistent state of its own.

**Primary security responsibility:** On a model `LOAD` action, verify that
selected files in a local model repository directory match checksums declared
in the model's `model_repository_agents` configuration, and fail the load on a
mismatch. Its purpose is detecting accidental corruption of model files, not
defending against an adversary.

**Key interfaces and boundaries:**

- Input from the server: the model repository location (filesystem artifacts
  only) and the agent parameters from the model configuration. Each parameter
  key has the form `<algorithm>:<relative/path>` and each value is the expected
  hex digest.
- Input from the filesystem: the contents of the files named by those keys.
- Output: success, or a `TRITONSERVER_Error` returned to the server. A
  checksum mismatch error includes the file path and the expected and computed
  digests; an open failure error includes the file path and the `errno` text.
- Cryptography: OpenSSL (`libcrypto`) for MD5; the only supported algorithm.

**Repository Exposure Classification:** Public. This repository is publicly
visible on GitHub.

**Service Exposure Classification:** Internal-Isolated (medium confidence).
The agent runs inside the Triton server process on the host that serves the
models and exposes no interface of its own. Exposure depends on how the
embedding Triton deployment is exposed; the classification is a documentation
aid and not an official NVIDIA label.

## Threat Model

1. **Unvalidated file paths in configuration keys:** `ReadFile()` joins the
   model directory and the relative path taken from the parameter key without
   normalizing it or rejecting absolute paths and `..` segments. A party who
   can edit a model configuration can make the agent read files outside the
   model directory, and the mismatch error returns the computed digest of
   that file, which discloses information about it.
2. **Weak integrity primitive:** only MD5 is supported. MD5 is not collision
   resistant, and the digest sits in the same model repository as the files it
   protects. The agent therefore cannot detect deliberate tampering by anyone
   who can modify the model files and configuration together, and it provides
   no authenticity or provenance guarantee.
3. **Verification gaps (fail-open):** only files named in agent parameters are
   checked. A model with no checksum parameters, or with files not listed,
   loads unverified. Only the `LOAD` action is handled, and only for
   filesystem artifacts. Other artifact types are rejected rather than
   verified.
4. **Time-of-check to time-of-use:** the digest is computed during the agent's
   load action. The backend reads the model files later, so a file replaced
   between verification and use is loaded without being re-verified.
5. **Resource exhaustion and unsafe sizing:** the whole file is read into
   memory before hashing. A very large file can exhaust memory in the server
   process. `ReadFile()` passes the result of `tellg()` straight to
   `resize()` without validating it. For inputs where `tellg()` fails, such as
   a directory, the invalid size makes `resize()` throw a standard library
   exception, and an oversized file can throw `std::bad_alloc`. The load
   action catches only the agent's own `ErrorException`, so these exceptions
   are not turned into a model-load error and can escape the agent, which may
   terminate the server process. A blocking file such as a FIFO can also stall
   the model load.
6. **Information disclosure through error messages:** what an error exposes
   depends on the failure. A failure to open a file includes the relative path
   and the `strerror(errno)` text, but no digests. A checksum mismatch
   includes the relative path and both the expected and computed digests, but
   no `strerror(errno)` text. These errors propagate to server logs and to
   clients that request model loads.

## Critical Security Assumptions

- The model repository and its configuration are **trusted** and writable only
  by trusted administrators. Anyone who can write them can alter both the
  files and the expected digests.
- Model configuration is authored by a trusted party. The agent does not
  sanitize paths or limit file sizes.
- The agent is used to detect accidental corruption such as partial copies or
  storage faults. Deployments that need protection against tampering should use
  stronger mechanisms, such as signed artifacts, and not rely on this example.
- The filesystem holding the model repository is not modified between
  verification and model load.
- Access control, authentication, TLS, and request authorization are provided
  by the embedding Triton Inference Server and its deployment environment.
- The OpenSSL library linked at build time is up to date and correctly
  installed.
- This project is provided as an example agent. Review it against your threat
  model before using it in production.
