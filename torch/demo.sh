#!/bin/bash
#

__NV_PRIME_RENDER_OFFLOAD=1 \
__VK_LAYER_NV_optimus=NVIDIA_only \
__NV_PRIME_RENDER_OFFLOAD_PROVIDER=NVIDIA-G0 \
__GLX_VENDOR_LIBRARY_NAME=nvidia \
CUDA_VISIBLE_DEVICES=0 \
    bin/torchdemo -d ~/Public/mnist -l 5 -b 64 -n 100 -e 2
