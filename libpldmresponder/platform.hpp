#pragma once

#include "config.h"

#include "handler.hpp"
#include "libpldmresponder/pdr.hpp"
#include "libpldmresponder/pdr_utils.hpp"
#include "utils.hpp"

#include <stdint.h>

#include <map>

#include "libpldm/platform.h"
#include "libpldm/states.h"

namespace pldm
{
namespace responder
{
namespace platform
{

using namespace pldm::utils;
using namespace pldm::responder::pdr_utils;

using generatePDR =
    std::function<void(const Json& json, pdr_utils::RepoInterface& repo)>;

using EffecterId = uint16_t;
using DbusObjMaps =
    std::map<EffecterId,
             std::tuple<pdr_utils::DbusMappings, pdr_utils::DbusValMaps>>;

class Handler : public CmdHandler
{
  public:
    Handler(const std::string& dir, pldm_pdr* repo) : pdrRepo(repo)
    {
        generate(dir, pdrRepo);

        handlers.emplace(PLDM_GET_PDR,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->getPDR(request, payloadLength);
                         });
        handlers.emplace(PLDM_SET_NUMERIC_EFFECTER_VALUE,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->setNumericEffecterValue(
                                 request, payloadLength);
                         });
        handlers.emplace(PLDM_SET_STATE_EFFECTER_STATES,
                         [this](const pldm_msg* request, size_t payloadLength) {
                             return this->setStateEffecterStates(request,
                                                                 payloadLength);
                         });
    }

    pdr_utils::Repo& getRepo()
    {
        return this->pdrRepo;
    }

    /** @brief Add D-Bus mapping and value mapping(stateId to D-Bus) for the
     *         effecterId. If the same id is added, the previous dbusObjs will
     *         be "over-written".
     *
     *  @param[in] effecterId - effecter id
     *  @param[in] dbusObj - list of D-Bus object structure and list of D-Bus
     *                       property value to attribute value
     */
    void addDbusObjMaps(
        uint16_t effecterId,
        std::tuple<pdr_utils::DbusMappings, pdr_utils::DbusValMaps> dbusObj);

    /** @brief Retrieve an effecter id -> D-Bus objects mapping
     *
     *  @param[in] effecterId - effecter id
     *
     *  @return std::tuple<pdr_utils::DbusMappings, pdr_utils::DbusValMaps> -
     *          list of D-Bus object structure and list of D-Bus property value
     *          to attribute value
     */
    const std::tuple<pdr_utils::DbusMappings, pdr_utils::DbusValMaps>&
        getDbusObjMaps(uint16_t effecterId) const;

    uint16_t getNextEffecterId()
    {
        return ++nextEffecterId;
    }

    /** @brief Parse PDR JSONs and build PDR repository
     *
     *  @param[in] dir - directory housing platform specific PDR JSON files
     *  @param[in] repo - instance of concrete implementation of Repo
     */
    void generate(const std::string& dir, Repo& repo);

    /** @brief Parse PDR JSONs and build state effecter PDR repository
     *
     *  @param[in] json - platform specific PDR JSON files
     *  @param[in] repo - instance of state effecter implementation of Repo
     */
    void generateStateEffecterRepo(const Json& json, Repo& repo);

    /** @brief Handler for GetPDR
     *
     *  @param[in] request - Request message payload
     *  @param[in] payloadLength - Request payload length
     *  @param[out] Response - Response message written here
     */
    Response getPDR(const pldm_msg* request, size_t payloadLength);

    /** @brief Handler for setNumericEffecterValue
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @return Response - PLDM Response message
     */
    Response setNumericEffecterValue(const pldm_msg* request,
                                     size_t payloadLength);

    /** @brief Handler for setStateEffecterStates
     *
     *  @param[in] request - Request message
     *  @param[in] payloadLength - Request payload length
     *  @return Response - PLDM Response message
     */
    Response setStateEffecterStates(const pldm_msg* request,
                                    size_t payloadLength);

  private:
    pdr_utils::Repo pdrRepo;
    uint16_t nextEffecterId{};
    DbusObjMaps dbusObjMaps{};
};

} // namespace platform
} // namespace responder
} // namespace pldm
