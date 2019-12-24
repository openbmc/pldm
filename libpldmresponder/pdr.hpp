#pragma once

#include "effecters.hpp"
#include "utils.hpp"

#include <stdint.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <xyz/openbmc_project/Common/error.hpp>

#include "libpldm/platform.h"

using InternalFailure =
    sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
namespace fs = std::filesystem;

namespace pldm
{

namespace responder
{

namespace pdr
{

using Type = uint8_t;
using Json = nlohmann::json;
using RecordHandle = uint32_t;
using Entry = std::vector<uint8_t>;
using Pdr = std::vector<Entry>;

/** @class Repo
 *
 *  @brief Abstract class describing the interface API to the PDR repository
 *
 *  Concrete implementations of this must handle storing and addressing the
 *  PDR entries by a "record handle", which can be indices, offsets, etc.
 */
class Repo
{
  public:
    virtual ~Repo() = default;

    /** @brief Add a new entry to the PDR
     *
     *  @param[in] entry - new PDR entry
     */
    virtual void add(Entry&& entry) = 0;

    /** @brief Access PDR entry at inout record handle
     *
     *  @param[in] handle - record handle
     *
     *  @return Entry - PDR entry
     */
    virtual Entry at(RecordHandle handle) const = 0;

    /** @brief Get next available record handle for assignment
     *
     *  @return RecordHandle - PDR record handle
     */
    virtual RecordHandle getNextRecordHandle() const = 0;

    /** @brief Get record handle immediately suceeding the input record
     *         handle
     *
     *  @param[in] current - input record handle
     *
     *  @return RecordHandle - PDR record handle
     */
    virtual RecordHandle getNextRecordHandle(RecordHandle current) const = 0;

    /** @brief Get number of entries in the PDR
     *
     *  @return size_t - number of entries
     */
    virtual size_t numEntries() const = 0;

    /** @brief Check if PDR is empty
     *
     *  @return bool - true if PDR is empty, false otherwise
     */
    virtual bool empty() const = 0;

    /** @brief Empty the PDR
     */
    virtual void makeEmpty() = 0;
};

namespace internal
{

/** @brief Parse PDR JSON file and output Json object
 *
 *  @param[in] path - path of PDR JSON file
 *
 *  @return Json - Json object
 */
inline Json readJson(const std::string& path)
{
    std::ifstream jsonFile(path);
    if (!jsonFile.is_open())
    {
        std::cout << "Error opening PDR JSON file, PATH=" << path.c_str()
                  << std::endl;
        return {};
    }

    return Json::parse(jsonFile);
}

/** @class IndexedRepo
 *
 *  @brief Inherits and implements Repo
 *
 *  Stores the PDR as a vector of entries, and addresses PDR entries based on an
 *  incrementing record handle, starting at 1.
 */
class IndexedRepo : public Repo
{
  public:
    void add(Entry&& entry)
    {
        repo.emplace_back(std::move(entry));
    }

    Entry at(RecordHandle handle) const
    {
        if (!handle)
        {
            handle = 1;
        }
        return repo.at(handle - 1);
    }

    RecordHandle getNextRecordHandle() const
    {
        return repo.size() + 1;
    }

    RecordHandle getNextRecordHandle(RecordHandle current) const
    {
        if (current >= repo.size())
        {
            return 0;
        }
        if (!current)
        {
            current = 1;
        }
        return current + 1;
    }

    size_t numEntries() const
    {
        return repo.size();
    }

    bool empty() const
    {
        return repo.empty();
    }

    void makeEmpty()
    {
        repo.clear();
    }

  private:
    Pdr repo{};
};

/** @brief Parse PDR JSONs and build PDR repository
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *  @tparam[in] repo - instance of concrete implementation of Repo
 */
template <typename T>
void generate(const std::string& dir, T& repo)
{
    using namespace internal;
    // A map of PDR type to a lambda that handles creation of that PDR type.
    // The lambda essentially would parse the platform specific PDR JSONs to
    // generate the PDR structures. This function iterates through the map to
    // invoke all lambdas, so that all PDR types can be created.
    std::map<Type, std::function<void(const Json& json, T& repo)>> generators =
        {{PLDM_STATE_EFFECTER_PDR, [](const auto& json, T& repo) {
              static const std::vector<Json> emptyList{};
              static const Json empty{};
              auto entries = json.value("entries", emptyList);
              for (const auto& e : entries)
              {
                  size_t pdrSize = 0;
                  auto effecters = e.value("effecters", emptyList);
                  static const Json empty{};
                  for (const auto& effecter : effecters)
                  {
                      auto set = effecter.value("set", empty);
                      auto statesSize = set.value("size", 0);
                      if (!statesSize)
                      {
                          std::cerr
                              << "Malformed PDR JSON - no state set info, TYPE="
                              << PLDM_STATE_EFFECTER_PDR << "\n";
                          throw InternalFailure();
                      }
                      pdrSize += sizeof(state_effecter_possible_states) -
                                 sizeof(bitfield8_t) +
                                 (sizeof(bitfield8_t) * statesSize);
                  }
                  pdrSize += sizeof(pldm_state_effecter_pdr) - sizeof(uint8_t);

                  Entry pdrEntry{};
                  pdrEntry.resize(pdrSize);

                  pldm_state_effecter_pdr* pdr =
                      reinterpret_cast<pldm_state_effecter_pdr*>(
                          pdrEntry.data());
                  pdr->hdr.record_handle = repo.getNextRecordHandle();
                  pdr->hdr.version = 1;
                  pdr->hdr.type = PLDM_STATE_EFFECTER_PDR;
                  pdr->hdr.record_change_num = 0;
                  pdr->hdr.length = pdrSize - sizeof(pldm_pdr_hdr);

                  pdr->terminus_handle = 0;
                  pdr->effecter_id = effecter::nextId();
                  pdr->entity_type = e.value("type", 0);
                  pdr->entity_instance = e.value("instance", 0);
                  pdr->container_id = e.value("container", 0);
                  pdr->effecter_semantic_id = 0;
                  pdr->effecter_init = PLDM_NO_INIT;
                  pdr->has_description_pdr = false;
                  pdr->composite_effecter_count = effecters.size();

                  using namespace effecter::dbus_mapping;
                  Paths paths{};
                  uint8_t* start = pdrEntry.data() +
                                   sizeof(pldm_state_effecter_pdr) -
                                   sizeof(uint8_t);
                  for (const auto& effecter : effecters)
                  {
                      auto set = effecter.value("set", empty);
                      state_effecter_possible_states* possibleStates =
                          reinterpret_cast<state_effecter_possible_states*>(
                              start);
                      possibleStates->state_set_id = set.value("id", 0);
                      possibleStates->possible_states_size =
                          set.value("size", 0);

                      start += sizeof(possibleStates->state_set_id) +
                               sizeof(possibleStates->possible_states_size);
                      static const std::vector<uint8_t> emptyStates{};
                      auto states = set.value("states", emptyStates);
                      for (const auto& state : states)
                      {
                          auto index = state / 8;
                          auto bit = state - (index * 8);
                          bitfield8_t* bf =
                              reinterpret_cast<bitfield8_t*>(start + index);
                          bf->byte |= 1 << bit;
                      }
                      start += possibleStates->possible_states_size;

                      auto dbus = effecter.value("dbus", empty);
                      paths.emplace_back(std::move(dbus));
                  }
                  add(pdr->effecter_id, std::move(paths));
                  repo.add(std::move(pdrEntry));
              }
          }}};

    auto eraseLen = strlen(".json");
    Type pdrType{};
    for (const auto& dirEntry : fs::directory_iterator(dir))
    {
        try
        {
            auto json = readJson(dirEntry.path().string());
            if (!json.empty())
            {
                auto fileName = dirEntry.path().filename().string();
                fileName.erase(fileName.end() - eraseLen);
                pdrType = stoi(fileName);
                generators.at(pdrType)(json, repo);
            }
        }
        catch (const InternalFailure& e)
        {
        }
        catch (const Json::exception& e)
        {
            std::cerr << "Failed parsing PDR JSON file, TYPE= " << pdrType
                      << " ERROR=" << e.what() << "\n";
            pldm::utils::reportError(
                "xyz.openbmc_project.bmc.pldm.InternalFailure");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed parsing PDR JSON file, TYPE= " << pdrType
                      << " ERROR=" << e.what() << "\n";
            pldm::utils::reportError(
                "xyz.openbmc_project.bmc.pldm.InternalFailure");
        }
    }
}

} // namespace internal

/** @brief Build (if not built already) and retrieve PDR
 *
 *  @param[in] dir - directory housing platform specific PDR JSON files
 *
 *  @return Repo& - Reference to instance of pdr::Repo
 */
Repo& get(const std::string& dir);

} // namespace pdr
} // namespace responder
} // namespace pldm
