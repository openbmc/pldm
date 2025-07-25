#include "utils.hpp"

namespace pldm::rde
{
void logCompletionCodeError(uint8_t cc)
{
    std::string message;

    switch (cc)
    {
        case PLDM_RDE_BAD_CHECKSUM:
            message = "BAD_CHECKSUM: The payload checksum is incorrect";
            break;
        case PLDM_RDE_CANNOT_CREATE_OPERATION:
            message = "CANNOT_CREATE_OPERATION: Unable to create operation";
            break;
        case PLDM_RDE_NOT_ALLOWED:
            message = "NOT_ALLOWED: Operation not permitted";
            break;
        case PLDM_RDE_WRONG_LOCATION_TYPE:
            message = "WRONG_LOCATION_TYPE: Invalid location type specified";
            break;
        case PLDM_RDE_ERROR_OPERATION_ABANDONED:
            message = "OPERATION_ABANDONED: Operation was aborted unexpectedly";
            break;
        case PLDM_RDE_OPERATION_UNKILLABLE:
            message = "OPERATION_UNKILLABLE: Cannot forcibly cancel operation";
            break;
        case PLDM_RDE_ERROR_OPERATION_EXISTS:
            message = "OPERATION_EXISTS: Duplicate operation detected";
            break;
        case PLDM_RDE_ERROR_OPERATION_FAILED:
            message = "OPERATION_FAILED: Operation execution failed";
            break;
        case PLDM_RDE_ERROR_UNEXPECTED:
            message = "UNEXPECTED_ERROR: Internal or unknown error occurred";
            break;
        case PLDM_RDE_ERROR_UNSUPPORTED:
            message = "UNSUPPORTED: Command or resource not supported";
            break;
        case PLDM_RDE_ERROR_UNRECOGNIZED_CUSTOM_HEADER:
            message =
                "UNRECOGNIZED_CUSTOM_HEADER: Header format not recognized";
            break;
        case PLDM_RDE_ERROR_ETAG_MATCH:
            message = "ETAG_MATCH_FAILED: ETag comparison mismatch";
            break;
        case PLDM_RDE_ERROR_NO_SUCH_RESOURCE:
            message = "NO_SUCH_RESOURCE: Referenced resource was not found";
            break;
        case PLDM_RDE_ERROR_ETAG_CALCULATION_ONGOING:
            message = "ETAG_CALCULATION_ONGOING: ETag generation in progress";
            break;
        default:
            message = "Unknown CompletionCode: cc=" + std::to_string(cc);
            break;
    }

    error("handleOperationInitResp failed with CompletionCode {CC}: {MSG}",
          "CC", cc, "MSG", message);
}

void logHexPayload(const std::vector<uint8_t>& payload)
{
    std::ostringstream oss;
    for (uint8_t byte : payload)
    {
        oss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
            << static_cast<int>(byte) << " ";
    }
    lg2::info("Payload HEX dump: {BYTES}", "BYTES", oss.str());
}
} // namespace pldm::rde
