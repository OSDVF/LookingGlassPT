#include <stdexcept>
class alert_exception : public std::runtime_error {
	using std::runtime_error::runtime_error;
};