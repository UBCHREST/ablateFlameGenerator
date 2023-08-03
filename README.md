# ABLATE Flame Generator

This executable uses ABLATE to generate a flame of flames based upon the specified input file. This code assumes that
a !ablate::solver::SteadyStateStepper is used for the time stepper.

Additional ABLATE Flame Generator options can be specified in the input file root as:

```yaml
# flameGenerator specific inputs
flameGenerator:
  maxNumberFlames: 1000 # the max number of flame that are created
  scaleFactor: 0.95 # the scaling factor between flames
```

An example input file is

```yaml
# This example will run a diffusion flame until the solution reaches quasi steady state as
# defined by the criteria list given to the time stepper
---
environment:
  title: _steadyStateDiffusionFlame
  tagDirectory: false
arguments: { }
# flameGenerator specific inputs
flameGenerator:
  maxNumberFlames: 1000
  scaleFactor: 0.95

# Create a stead state stepper.  The Stead State Stepper marches the solution in time until a set of criteria is met
timestepper: !ablate::solver::SteadyStateStepper
  # The Steady state stepper can output using the standard serializers
  io: !ablate::io::Hdf5MultiFileSerializer
    # results are saved at every 0 steps.  In real simulations this should be much larger.
    interval: 0
  # Pass the standard petsc arguments to the TS.
  arguments:
    ts_type: rk
    ts_max_time: 1
    ts_max_steps: 500000 # If convergence is not reached by ts_max_steps an exception is thrown
    ts_dt: 1.0e-10
    ts_adapt_safety: 0.75
  # A list of criteria can be given to the steady state stepper.  All criteria must be met to be considered converged
  criteria:
    # The VariableChange measures the norm between the current and previous variable state.  If less than the tolerance
    # the variable is considered converged
    - !ablate::solver::criteria::VariableChange
      name: temperature
      tolerance: 500 # This should be smaller for non-test cases
      norm: l2_norm
    # The valid range does not check for convergence but throws an error is all points fall outside the valid range.
    # This is useful for checking for invalid results such as flame extinguishment
    - !ablate::solver::criteria::ValidRange
      name: temperature
      lowerBound: 500
      upperBound: 100000
  # state how many time steps will be completed between criteria checks
  checkInterval: 10
  # the steady state steppers allows for a log to monitor convergence rate
  log: !ablate::monitors::logs::StdOut

  # Create a 1D BoxMeshBoundaryCells to monitor the solution
  domain: !ablate::domain::BoxMeshBoundaryCells
    name: simpleBoxField
    faces: [ 200 ]
    lower: [ 0.0 ]
    upper: [ 0.01 ]
    simplex: false
    options:
      dm_plex_hash_location: true
    preModifiers:
      # distribute the mesh across the mpi rank with ghost cells
      - !ablate::domain::modifiers::DistributeWithGhostCells
        ghostCellDepth: 2
    postModifiers:
      - !ablate::domain::modifiers::TagLabelInterface
        # tag the left boundary faces needed to remove the boundary from the interiorFlowRegion
        leftRegion:
          name: boundaryCellsLeft
        rightRegion:
          name: domain
        boundaryFaceRegion:
          name: boundaryFaceLeft
      - !ablate::domain::modifiers::SubtractLabel
        # remove the slabBurnerFace from the flowRegion
        differenceRegion:
          name: interiorFlowRegion
        minuendRegion:
          name: interiorCells
        subtrahendRegions:
          - name: boundaryFaceLeft
        incompleteLabel: true
      - !ablate::domain::modifiers::GhostBoundaryCells
      # the DmViewFromOptions should output once with full mesh/dm details
    fields:
      - !ablate::finiteVolume::CompressibleFlowFields
        # create an ideal gas eos using the tchem library
        eos: !ablate::eos::TChem  &eos
          mechFile: MMAReduced.yaml
          options:
            # set a minimum temperature for the chemical kinetics ode integration
            thresholdTemperature: 560
        conservedFieldOptions:
          petscfv_type: leastsquares
        region:
          name: domain
      - !ablate::domain::FieldDescription
        name: pressure
        type: FV
        location: aux
        region:
          name: domain
  # provide an initial condition for the flow field
  initialization:
    # Set up the euler field (density, density*energy, density*vel) based upon temperature, pressure, velocity and mass fractions
    - !ablate::finiteVolume::fieldFunctions::Euler
      state: &flowFieldState
        eos: *eos
        temperature:
          "x < 0 ? 653.0  : (x > .01 ? 300.0 : (x < 0.007 ? (306714.2857*x + 653): (-833333.3333*x + 8633.33)  ))"
        pressure: 101325.0
        velocity: "0.0"
        other: !ablate::finiteVolume::fieldFunctions::MassFractions
          &massFracs
          eos: *eos
          values:
            - fieldName: O2
              field: !ablate::mathFunctions::Linear
                startValues: [ 0.0 ]
                endValues: [ 1.0 ]
                end: .01

            - fieldName: MMETHAC_C5H8O2
              field: !ablate::mathFunctions::Linear
                startValues: [ 1.0 ]
                endValues: [ 0.0 ]
                end: .01

    # initialize the conserved mass fractions using the same flow staet
    - !ablate::finiteVolume::fieldFunctions::DensityMassFractions
      state: *flowFieldState

# set the solvers diffusion flame
solvers:
  # The compressible flow solver is required to solve diffusion and reactions in the flow
  - !ablate::finiteVolume::CompressibleFlowSolver
    id: flowField
    # only apply this solver on the main region (no bc cells)
    region:
      name: interiorFlowRegion
    # the flow solver requires the equation of state to compute properties
    eos: *eos
    # Allow transport used the Sutherland model
    transport: !ablate::eos::transport::Sutherland
      eos: *eos
    # in addition to the standard processes add chemistry reactions and a pressure fix to keep the diffusion flame at 1 ATM
    additionalProcesses:
      - !ablate::finiteVolume::processes::Chemistry
        eos: *eos
      - !ablate::finiteVolume::processes::ConstantPressureFix
        eos: *eos
        pressure: 101325.0

  # use a fixed inlet to enforce boundary conditions on the right side
  - !ablate::boundarySolver::BoundarySolver
    id: walls
    region:
      name: boundaryCellsRight
    fieldBoundary:
      name: boundaryFaces
    mergeFaces: true
    processes:
      - !ablate::boundarySolver::lodi::Inlet
        eos: *eos

  # use a fixed inlet to enforce boundary conditions on the left side
  - !ablate::boundarySolver::BoundarySolver
    id: slab boundary
    region:
      name: boundaryCellsLeft
    fieldBoundary:
      name: boundaryFaceLeft
    processes:
      - !ablate::boundarySolver::lodi::Inlet
        eos: *eos


```

# Installation

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

## Downloading and Building with CLion

CLion is a C/C++ IDE that uses cmake files for configuration. These directions outline the steps to running the
framework with CLion.

1. Download and Install [CLion](https://www.jetbrains.com/clion/).
2. Open CLion and select *Get From VCS* from the welcome window and either
    - (recommended) Select GitHub and Login/Authorize access. Then follow on-screen instructions to clone your repo of
      ABLATE Flame Generator,
    - Select Git from the *Version Control* dropdown and enter your ABLATE Flame Generator Repo.
3. Enable the ```ablate-flame-generator-debug``` and ```ablate-flame-generator-opt``` profiles.
    - If not opened by default, open the Settings / Preferences > Build, Execution, Deployment > CMake preference window
      from the menu bar.
    - Select the ```ablate-flame-generator-debug``` and click the "Enable profile". Repeat for
      the ```ablate-flame-generator-opt``` and
      apply/close the window.
    - Select the ```ablate-flame-generator-debug``` or ```ablate-flame-generator-opt``` build profile under the build
      toolbar. In short,
      the debug build makes it easier to debug but is slower. The release/optimized build is faster to execute.
    - Disable any other profile
4. Select the ablateFlameGenerator configuration and add ```--input /path/to/input/file.yaml``` to the Program Arguments
   under configuration.
5. If you are new to CLion it is recommended that you read through
   the [CLion Quick Start Guide](https://www.jetbrains.com/help/clion/clion-quick-start-guide.html).

## Downloading and Building with the Command Line

1. Clone your ABLATE client Repo onto your local machine. It is recommended that
   you [setup passwordless ssh](https://docs.github.com/en/github/authenticating-to-github/adding-a-new-ssh-key-to-your-github-account)
   for accessing GitHub.
2. Move into the ablate client directory
3. Configure and build ABLATE FLA client. Both the debug and optimized versions are built. If you are developing new
   capabilities you may want to specify debug. If you are running large simulations specify opt.
    ```bash
    # debug mode
    cmake  --preset=ablate-flame-generator-debug
    cmake --build --preset=ablate-flame-generator-debug-build -j

    # optimized
    cmake  --preset=ablate-flame-generator-debug-build
    cmake --build --preset=ablate-flame-generator-opt-build -j
    ```
4. Run the client executable (You may have changed the name of the executable)
    ```bash
    # debug mode
    $PETSC_DIR/arch-ablate-debug/bin/mpirun -n 1 cmake-build-debug/ablateFlameGenerator --input /path/to/input/file.yaml

    # optimized
    $PETSC_DIR/arch-ablate-opt/bin/mpirun -n 1 cmake-build-opt/ablateFlameGenerator  --input /path/to/input/file.yaml
    ```   