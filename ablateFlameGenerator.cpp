#include <petsc.h>
#include <domain/boxMesh.hpp>
#include <domain/boxMeshBoundaryCells.hpp>
#include <environment/download.hpp>
#include <environment/runEnvironment.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include "builder.hpp"
#include "localPath.hpp"
#include "parameters/mapParameters.hpp"
#include "steadyStateStepper.hpp"
#include "utilities/mpiUtilities.hpp"
#include "utilities/petscUtilities.hpp"
#include "version.h"
#include "yamlParser.hpp"

int main(int argc, char** argv) {
    // initialize petsc and mpi
    ablate::environment::RunEnvironment::Initialize(&argc, &argv);
    ablate::utilities::PetscUtilities::Initialize();

    {
        // print version information
        PetscBool printInfo = PETSC_FALSE;
        PetscOptionsGetBool(nullptr, nullptr, "-version", &printInfo, nullptr) >> ablate::utilities::PetscUtilities::checkError;
        if (!printInfo) {
            PetscOptionsGetBool(nullptr, nullptr, "--info", &printInfo, nullptr) >> ablate::utilities::PetscUtilities::checkError;
        }
        if (printInfo) {
            std::cout << "ABLATE Flame Generator" << std::endl;
            std::cout << '\t' << "Version: " << ABLATEFLAMEGENERATOR_VERSION << std::endl;
            std::cout << "----------------------------------------" << std::endl;

            ablate::Builder::PrintInfo(std::cout);
            std::cout << "----------------------------------------" << std::endl;
        }

        PetscBool printVersion = PETSC_FALSE;
        PetscOptionsGetBool(nullptr, nullptr, "--version", &printVersion, nullptr) >> ablate::utilities::PetscUtilities::checkError;
        if (printVersion) {
            std::cout << ABLATEFLAMEGENERATOR_VERSION << std::endl;
            return 0;
        }

        // use a standard input file to generate the domain
        char filename[PETSC_MAX_PATH_LEN] = "";
        PetscBool fileSpecified = PETSC_FALSE;
        PetscOptionsGetString(nullptr, nullptr, "--input", filename, PETSC_MAX_PATH_LEN, &fileSpecified) >> ablate::utilities::PetscUtilities::checkError;
        if (!fileSpecified) {
            throw std::invalid_argument("the --input must be specified");
        }

        // locate or download the file
        std::filesystem::path filePath;
        if (ablate::environment::Download::IsUrl(filename)) {
            ablate::environment::Download downloader(filename);
            filePath = downloader.Locate();
        } else {
            cppParser::LocalPath locator(filename);
            filePath = locator.Locate();
        }
        // make sure that the input file can be found
        if (!std::filesystem::exists(filePath)) {
            throw std::invalid_argument("unable to locate input file: " + filePath.string());
        }

        {
            // load in the base mesh and input file
            std::shared_ptr<cppParser::YamlParser> parser = std::make_shared<cppParser::YamlParser>(filePath);

            // setup the monitor
            auto setupEnvironmentParameters = parser->GetByName<ablate::parameters::Parameters>("environment");
            ablate::environment::RunEnvironment::Setup(*setupEnvironmentParameters, filePath);

            // Copy over the input file
            int rank;
            MPI_Comm_rank(PETSC_COMM_WORLD, &rank) >> ablate::utilities::MpiUtilities::checkError;
            if (rank == 0) {
                std::filesystem::path inputCopy = ablate::environment::RunEnvironment::Get().GetOutputDirectory() / filePath.filename();
                std::ofstream stream(inputCopy);
                parser->Print(stream);
                stream.close();
            }

            // create the base/old flame
            auto oldFlameStepper = std::dynamic_pointer_cast<ablateFlameGenerator::SteadyStateStepper>(ablate::Builder::Build(parser));
            // make sure the stepper is an ablateFlameGenerator::SteadyStateStepper
            if (oldFlameStepper == nullptr) {
                throw std::invalid_argument("The TimeStepper must be ablateFlameGenerator::SteadyStateStepper");
            }

            // Do some sanity checks to make sure the classes are what we think they should be
            const auto& domain = oldFlameStepper->GetDomain();
            try {
                [[maybe_unused]] auto& domainCheck = dynamic_cast<const ablate::domain::BoxMeshBoundaryCells&>(domain);
            } catch (std::bad_cast&) {
                throw std::invalid_argument("The Domain must be of type !ablate::domain::BoxMeshBoundaryCells");
            }

            // Now march the time stepper until it is converged

        }
    }

    ablate::environment::RunEnvironment::Finalize();
    return 0;
}