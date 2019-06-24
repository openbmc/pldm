#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <string>

using CallbackName = std::string;
using CallbackFunction = std::function<void()>;
using CallbackMap = std::map<CallbackName, CallbackFunction>;

/**
 * This macro can be used in each callback to make it
 * available to the pldmtool executable.
 */
#define REGISTER_CALLBACK(name, func)                                          \
    namespace func##_ns                                                        \
    {                                                                          \
        Registration r{std::move(name), std::move(func)};                      \
    }

/**
 * Used to register callback.  Each callback function can then
 * be found in a map via its name.
 */
class Registration
{
  public:
    /**
     *  Adds the callback name and function to the internal
     *  callback map.
     *
     *  @param[in] name - the callback name
     *  @param[in] function - the function to run
     */
    Registration(CallbackName&& name, CallbackFunction&& function)
    {
        callbacks.emplace(std::move(name), std::move(function));
    }

    /**
     * Returns the map of callbacks
     */
    static const CallbackMap& getCallbacks()
    {
        return callbacks;
    }

  private:
    static CallbackMap callbacks;
};
