#include "steadyStateStepper.hpp"

#include <utility>

ablateFlameGenerator::SteadyStateStepper::SteadyStateStepper(std::shared_ptr<ablate::domain::Domain> domain,
                                                             std::vector<std::shared_ptr<ablateFlameGenerator::ConvergenceCriteria>> convergenceCriteria,
                                                             const std::shared_ptr<ablate::parameters::Parameters>& arguments, std::shared_ptr<ablate::io::Serializer> serializer,
                                                             std::shared_ptr<ablate::domain::Initializer> initialization,
                                                             std::vector<std::shared_ptr<ablate::mathFunctions::FieldFunction>> absoluteTolerances,
                                                             std::vector<std::shared_ptr<ablate::mathFunctions::FieldFunction>> relativeTolerances, bool verboseSourceCheck,
                                                             std::shared_ptr<ablate::monitors::logs::Log> log)
    : ablate::solver::TimeStepper(std::move(domain), arguments, nullptr /* no io for stead state solver */, std::move(initialization), {} /* no exact solution for stead state solver */,
                                  std::move(absoluteTolerances), std::move(relativeTolerances), verboseSourceCheck),
      convergenceCriteria(std::move(convergenceCriteria)),
      log(std::move(log)) {}

ablateFlameGenerator::SteadyStateStepper::~SteadyStateStepper() = default;

void ablateFlameGenerator::SteadyStateStepper::Solve() {
    // Do the basic initialize
    TimeStepper::Initialize();

    // Set the initial max time steps
    TSSetMaxSteps(GetTS(), stepsBetweenChecks) >> ablate::utilities::PetscUtilities::checkError;

    // perform a basic solve (this will set up anything else that needs to be setup)
    TimeStepper::Solve();

    // keep stepping until convergence is reached
    bool converged = false;

    // step until convergence is reached
    if (!converged) {
        // Solve to the next number of steps
        TSSolve(GetTS(), nullptr) >> ablate::utilities::PetscUtilities::checkError;

        // check for convergence.  Set converged to true before checking
        converged = true;

        // Get the current step
        PetscInt step;
        TSGetStepNumber(GetTS(), &step) >> ablate::utilities::PetscUtilities::checkError;

        for (auto& criterion : convergenceCriteria) {
            converged = converged && criterion->CheckConvergence(GetDomain(), step, log);
        }

        // report log status
        if (log) {
            if (converged) {
                log->Printf("Convergence reached after %" PetscInt_FMT "steps", step);
            }
            if (converged) {
                log->Printf("Solution not converged after %" PetscInt_FMT " steps.", step);
            }
        }

        // Increase the number of steps and try again
        TSSetMaxSteps(GetTS(), stepsBetweenChecks + step) >> ablate::utilities::PetscUtilities::checkError;
    }
}

#include "registrar.hpp"
REGISTER(ablate::solver::TimeStepper, ablateFlameGenerator::SteadyStateStepper, "a time stepper designed to march to steady state",
         ARG(ablate::domain::Domain, "domain", "the mesh used for the simulation"),
         OPT(std::vector<ablateFlameGenerator::ConvergenceCriteria>, "criteria", "the criteria used to determine when the solution is converged"),
         OPT(ablate::parameters::Parameters, "arguments", "arguments to be passed to petsc"), OPT(ablate::io::Serializer, "io", "the serializer used with this timestepper"),
         OPT(ablate::domain::Initializer, "initialization", "initialization field functions"),
         OPT(std::vector<ablate::mathFunctions::FieldFunction>, "absoluteTolerances", "optional absolute tolerances for a field"),
         OPT(std::vector<ablate::mathFunctions::FieldFunction>, "relativeTolerances", "optional relative tolerances for a field"),
         OPT(bool, "verboseSourceCheck", "does a slow nan/inf for solvers that use rhs evaluation. This is slow and should only be used for debug."),
         OPT(ablate::monitors::logs::Log, "log", "optionally log the convergence history"));
