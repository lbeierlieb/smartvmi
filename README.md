# SmartVMI adaptation for bpbench

This repository has been forked from [the official SmartVMI](https://github.com/GDATASoftwareAG/smartvmi) to work
together with [bpbench](https://github.com/lbeierlieb/bpbench) in an effort to quantify the overhead of VMI
breakpoints.

Specifically, this branch is supposed to be used for measuring the overhead that occurs when SmartVMI is notified
about every CR3 change.

Usage:
* run smartvmi
  * tells the hypervisor to notify about every CR3 write event - registered callback does nothing
  * it will look for bpbench.exe process
  * because it doesn't find it, no breakpoint will be injected
* run benchmark (e.g., [CPU-Z](https://www.cpuid.com/downloads/cpu-z/cpu-z_2.13-en.zip))
  * its performance will be impacted by context switches/CR3 writes taking longer, leaving less time for the benchmark
    to run
* stopping smartvmi unregisters the CR3 write monitoring
* compare benchmark results with and without smartvmi running

# SmartVMI

Virtual Machine Introspection (VMI) for memory forensics and machine-learning.

# SmartVMI Code

## VmiCore

The SmartVMI project is split into a core component which manages access to the virtual machine and provides a high
abstraction layer for ease of plugin implementation.
See [VmiCore Readme](vmicore/Readme.md) for additional information as well as how to build/use this project.

## Plugins

To allow for easy extension SmartVMI provides a plugin interface. For information about writing your own plugin see
the [Plugins Readme](plugins/Readme.md).
You can find already implemented plugins which also serve as examples for how to use this project in the plugins folder.
For additional information see the corresponding plugin readme:

* [Template](plugins/template/Readme.md) Stripped down plugin to take your first steps with plugin development.
* [InMemoryScanner](plugins/inmemoryscanner/Readme.md)
* [ApiTracing](plugins/apitracing/Readme.md)

# SmartVMI Research Project

The project “Synthesizing ML training data in the IT security domain for VMI-based attack detection and analysis” (
SmartVMI) is a research project funded by the BMBF and DLR.
See: [www.smartvmi.org](http://www.smartvmi.org) for more information.
