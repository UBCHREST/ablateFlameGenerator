# ABLATE Client Template

An example ABLATE client that illustrates using ABLATE in your application. See
the [ABLATE documentation](https://ablate.dev/content/development/ClientLibrary.html) for details about the library.

You must install PETSc following the instructions for you system on
the [ABLATE Build Wiki](https://github.com/UBCHREST/ablate/wiki) along with setting additional PETSc environmental
variable:

```bash
export PETSC_DIR="" #UPDATE to the real path of petsc
```

# Optional: Specify local ABLATE instead of using the latest

If developing features for ABLATE you may want to specify a local build of ABLATE_PATH instead of downloading it as an
environment variable.

```bash
export ABLATE_PATH="" #OPTIONAL path to ABLATE source directory/path/to/ablate/source/dir
```

## ownloading and Building with CLion

CLion is a C/C++ IDE that uses cmake files for configuration. These directions outline the steps to running the
framework with CLion.

1. Download and Install [CLion](https://www.jetbrains.com/clion/).
2. Open CLion and select *Get From VCS* from the welcome window and either
    - (recommended) Select GitHub and Login/Authorize access. Then follow on-screen instructions to clone your repo of
      ABLATE Client,
    - Select Git from the *Version Control* dropdown and enter your ABLATE Client Repo.
3. Enable the ```ablate-client-debug``` and ```ablate-client-opt``` build profiles.
    - If not opened by default, open the Settings / Preferences > Build, Execution, Deployment > CMake preference window
      from the menu bar.
    - Select the ```ablate-client-debug``` and click the "Enable profile". Repeat for the ```ablate-client-opt``` and
      apply/close the window.
      ![clion cmake profiles](assets/clion_cmake_profiles.png)
    - Select the ```ablate-client-opt``` or ```ablate-client-debug``` build profile under the build toolbar. In short,
      the debug build makes it easier to debug but is slower. The release/optimized build is faster to execute.
      ![clion cmake select build profile](assets/clion_cmake_select_build_profile.png)
    - Disable any other profile
4. If you are new to CLion it is recommended that you read through
   the [CLion Quick Start Guide](https://www.jetbrains.com/help/clion/clion-quick-start-guide.html).

## Downloading and Building with the Command Line

1. Clone your ABLATE client Repo onto your local machine. It is recommended that
   you [setup passwordless ssh](https://docs.github.com/en/github/authenticating-to-github/adding-a-new-ssh-key-to-your-github-account)
   for accessing GitHub.
2. Move into the ablate client directory
3. Configure and build ABLATE client. Both the debug and optimized versions are built. If you are developing new
   capabilities you may want to specify debug. If you are running large simulations specify opt.
    ```bash
    # debug mode
    cmake  --preset=ablate-client-debug
    cmake --build --preset=ablate-client-debug -j

    # optimized
    cmake  --preset=ablate-client-opt
    cmake --build --preset=ablate-client-opt -j
    ```
4. Run the client executable (You may have changed the name of the executable)
    ```bash
    # debug mode
    $PETSC_DIR/arch-ablate-debug/bin/mpirun -n 1 cmake-build-debug/ablateLibraryClient

    # optimized
    $PETSC_DIR/arch-ablate-opt/bin/mpirun -n 1 cmake-build-opt/ablateLibraryClient
    ```   