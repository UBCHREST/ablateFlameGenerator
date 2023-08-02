#ifndef ABLATEFLAMEGENERATOR_STEADYSTATESTEPPER_HPP
#define ABLATEFLAMEGENERATOR_STEADYSTATESTEPPER_HPP

#include <monitors/logs/log.hpp>
#include "convergenceCriteria.hpp"
#include "solver/timeStepper.hpp"

namespace ablateFlameGenerator {
class SteadyStateStepper : public ablate::solver::TimeStepper {
   private:
    /**
     * The number of steps between checks
     */
    PetscInt stepsBetweenChecks = 100;

    //! allow for multiple convergence checks
    std::vector<std::shared_ptr<ablateFlameGenerator::ConvergenceCriteria>> convergenceCriteria;

    //! optionally log the convergence history
    const std::shared_ptr<ablate::monitors::logs::Log> log = nullptr;

    //

   public:
    /**
     * constructor for steady state stepper to march the solution to steady state
     * @param domain
     * @param arguments
     * @param initialization
     * @param absoluteTolerances
     * @param relativeTolerances
     * @param verboseSourceCheck
     */
    explicit SteadyStateStepper(std::shared_ptr<ablate::domain::Domain> domain, std::vector<std::shared_ptr<ablateFlameGenerator::ConvergenceCriteria>> convergenceCriteria,
                                const std::shared_ptr<ablate::parameters::Parameters> &arguments = {}, std::shared_ptr<ablate::io::Serializer> serializer = {},
                                std::shared_ptr<ablate::domain::Initializer> initialization = {}, std::vector<std::shared_ptr<ablate::mathFunctions::FieldFunction>> absoluteTolerances = {},
                                std::vector<std::shared_ptr<ablate::mathFunctions::FieldFunction>> relativeTolerances = {}, bool verboseSourceCheck = {},
                                std::shared_ptr<ablate::monitors::logs::Log> log = {});

    /**
     * clean up any of the local memory
     */
    ~SteadyStateStepper() override;

    /**
     * Solves the system until steady state is achieved
     */
    void Solve() override;
};
}  // namespace ablateFlameGenerator

#endif  // ABLATEFLAMEGENERATOR_STEADYSTATESTEPPER_HPP
