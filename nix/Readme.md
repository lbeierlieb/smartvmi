# Building SmartVMI with Nix

You can also use Nix to reproducibly build VMICore and its plugins.

## How to Build

- Install the Nix package manager (https://nixos.org/download/) and enable Nix flakes (https://nixos.wiki/wiki/flakes)

- Clone this repository

- Inside the repository, run:

```console
[user@localhost repo_dir]$ nix build
```

- The build process creates the symlink `result`, which links to the folder with the build artifacts

## How to Run

Follow the guide from the [VMICore guide](../vmicore/Readme.md#how-to-run) with the binary located at
`result/bin/vmicore`.

Note: libvmi dynamically loads the libraries `libvirt.so`, `libkvmi.so`, `libxenstore.so`, and `libxenctrl.so`.
Nix provides these libraries, but, depending on your system, you might have to configure that the linker
loads these libraries from your system instead (e.g., libvirt v10.9 is running the VM on your system, but
the Nix build provides `libvirt.so` from libvirt v10.0—then, SmartVMI will not be able to connect to the VM).
Set the `LD_LIBRARY_PATH` environment variable, so that the dlopen can find the libraries from your system.

## Configuration

Follow the guide from the [VMICore guide](../vmicore/Readme.md#configuration).
The plugin binaries are located at `result/lib`.
