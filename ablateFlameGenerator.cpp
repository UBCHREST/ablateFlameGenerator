#include <petsc.h>
#include <domain/boxMeshBoundaryCells.hpp>
#include <environment/download.hpp>
#include <environment/runEnvironment.hpp>
#include <fstream>
#include <io/hdf5MultiFileSerializer.hpp>
#include <io/interval/fixedInterval.hpp>
#include <iostream>
#include <memory>
#include <parameters/petscPrefixOptions.hpp>
#include "builder.hpp"
#include "localPath.hpp"
#include "parameters/mapParameters.hpp"
#include "solver/steadyStateStepper.hpp"
#include "utilities/mpiUtilities.hpp"
#include "utilities/petscUtilities.hpp"
#include "version.h"
#include "yamlParser.hpp"

int main(int argc, char** argv) {
    // initialize petsc and mpi
    ablate::environment::RunEnvironment::Initialize(&argc, &argv);
    ablate::utilities::PetscUtilities::Initialize();

    // hard code the flame name
    std::string flameName = "flame_";

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
            // build options from the command line
            const char* replacementInputPrefix = "-yaml::";
            auto yamlOptions = std::make_shared<ablate::parameters::PetscPrefixOptions>(replacementInputPrefix);
            std::shared_ptr<cppParser::YamlParser> parser = std::make_shared<cppParser::YamlParser>(filePath, yamlOptions->GetMap());

            // Get the flameGenerator parameters
            auto flameGeneratorParameters = parser->GetByName<ablate::parameters::Parameters>("flameGenerator");
            std::size_t maxNumberOfFlames = flameGeneratorParameters->Get("maxNumberFlames", 10);
            double scaleFactor = flameGeneratorParameters->Get("scaleFactor", 0.85);

            // set up the monitor
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

            // Create hdf5 multi file serializer that will be used to output the flame
            auto baseOutputDirectory = ablate::environment::RunEnvironment::Get().GetOutputDirectory();

            // create a serializers to save all the flame result
            ablate::environment::RunEnvironment::Setup(ablate::environment::RunEnvironment::Parameters().Title("flames").TagDirectory(false).OutputDirectory(baseOutputDirectory / "flames"));
            std::filesystem::create_directory(baseOutputDirectory / "flames");
            auto flameSerializer = std::make_shared<ablate::io::Hdf5MultiFileSerializer>(std::make_shared<ablate::io::interval::FixedInterval>());

            // Keep track of the total scale factor
            double totalScaleFactor = 1.0;

            // keep track of the old stepper to copy over final conditions
            std::shared_ptr<ablate::solver::SteadyStateStepper> oldFlameStepper = nullptr;

            // March over each possible flame
            for (std::size_t flameId = 0; flameId < maxNumberOfFlames; ++flameId) {
                // Output information about this flame
                std::cout << "Starting flame " << flameId << std::endl;

                // Update the environment for this specific case
                auto title = flameName + std::to_string(flameId);
                ablate::environment::RunEnvironment::Setup(ablate::environment::RunEnvironment::Parameters().Title(title).TagDirectory(false).OutputDirectory(baseOutputDirectory / title));

                // create the base/old flame
                auto currentFlameStepper = std::dynamic_pointer_cast<ablate::solver::SteadyStateStepper>(ablate::Builder::Build(parser));
                // make sure the stepper is an ablateFlameGenerator::SteadyStateStepper
                if (currentFlameStepper == nullptr) {
                    throw std::invalid_argument("The TimeStepper must be ablateFlameGenerator::SteadyStateStepper");
                }

                // Set up the new time stepper
                currentFlameStepper->Initialize();

                // Copy over the old stepper if available
                if (oldFlameStepper) {
                    Vec oldSolutionVector = oldFlameStepper->GetSolutionVector();
                    Vec currentSolutionVector = currentFlameStepper->GetSolutionVector();
                    VecCopy(oldSolutionVector, currentSolutionVector) >> ablate::utilities::PetscUtilities::checkError;
                }

                // Now march the time stepper until it is converged
                currentFlameStepper->Solve();

                // Save this flame
                std::cout << "\tWriting results for flame " << flameId << std::endl;
                flameSerializer->Reset();
                currentFlameStepper->RegisterSerializableComponents(flameSerializer);
                flameSerializer->Serialize(currentFlameStepper->GetTS(), (PetscInt)flameId, (PetscReal)flameId, currentFlameStepper->GetSolutionVector());

                // update the scale factor
                totalScaleFactor *= scaleFactor;
                // Create an options object to scale
                auto scaleOptions = ablate::parameters::MapParameters::Create(yamlOptions->GetMap());
                scaleOptions->Insert("timestepper::domain::options::dm_plex_scale", totalScaleFactor);

                // reset the parser
                parser = std::make_shared<cppParser::YamlParser>(filePath, scaleOptions->GetMap());

                // store the current time stepper to use as a starting point for the new
                oldFlameStepper = currentFlameStepper;
            }
        }
    }

    ablate::environment::RunEnvironment::Finalize();
    return 0;
}