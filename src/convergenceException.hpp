#ifndef ABLATEFLAMEGENERATOR_CONVERGENCEEXCEPTION_HPP
#define ABLATEFLAMEGENERATOR_CONVERGENCEEXCEPTION_HPP

#include <exception>
#include <string>
#include <utility>

namespace ablateFlameGenerator {

/**
 * Custom exception for convergence type exceptions
 */
class ConvergenceException : public std::exception {
   private:
    std::string message;

   public:
    /**
     * Create a generic ConvergenceException with the given message
     * @param message
     */
    explicit ConvergenceException(std::string message): message(std::move(message)){}

    /**
     * return the specified message
     * @return
     */
    [[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
};

}  // namespace ablateFlameGenerator

#endif  // ABLATEFLAMEGENERATOR_CONVERGENCEEXCEPTION_HPP
