spirv-opt passes used for DXC
====

[SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) has a good
infrastructure to handle SPIR-V instructions. In particular, spirv-opt passes
conduct the IR transformation for the optimization and legalization. As time
goes, more and more DXC's SPIR-V visitors just duplicate the same infrastructure
in DXC. Since we implement the same thing again and again, we decide to
implement spirv-opt passes for visitors. Since they are specific for DXC usage
cases, we keep them in this folder. We will consider moving them to the upstream
SPIRV-Tools repo.
