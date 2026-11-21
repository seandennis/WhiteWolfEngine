#pragma once

#include <stdexcept>

/**
 * Thrown in place of terminating the process while an FScopedLogCapture is alive, so that tests can observe fatal
 * errors and failed assertions. The exception message is the text of the Fatal log message.
 */
class FFatalErrorException : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};
