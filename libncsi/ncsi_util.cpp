#include "ncsi_util.hpp"

#include <linux/ncsi.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>

#include <iostream>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace phosphor
{
namespace network
{
namespace ncsi
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

using CallBack = int (*)(struct nl_msg *msg, void *arg);

#define ETHERNET_HEADER_SIZE 16
int return_resp;

struct ncsi_pkt_hdr {
	unsigned char mc_id;	/* Management controller ID */
	unsigned char revision; /* NCSI version - 0x01      */
	unsigned char reserved; /* Reserved                 */
	unsigned char id;	/* Packet sequence number   */
	unsigned char type;	/* Packet type              */
	unsigned char channel;	/* Network controller ID    */
	__be16 length;		/* Payload length           */
	__be32 reserved1[2];	/* Reserved                 */
};

namespace internal
{

using nlMsgPtr = std::unique_ptr<nl_msg, decltype(&::nlmsg_free)>;
using nlSocketPtr = std::unique_ptr<nl_sock, decltype(&::nl_socket_free)>;

CallBack infoCallBack = [](struct nl_msg *msg, void * /*arg*/) {
	using namespace phosphor::network::ncsi;
	auto nlh = nlmsg_hdr(msg);

	struct nlattr *tb[NCSI_ATTR_MAX + 1] = {nullptr};
	struct nla_policy ncsiPolicy[NCSI_ATTR_MAX + 1] = {
	    {type : NLA_UNSPEC}, {type : NLA_U32}, {type : NLA_NESTED},
	    {type : NLA_U32},	 {type : NLA_U32},
	};

	struct nlattr *packagetb[NCSI_PKG_ATTR_MAX + 1] = {nullptr};
	struct nla_policy packagePolicy[NCSI_PKG_ATTR_MAX + 1] = {
	    {type : NLA_UNSPEC}, {type : NLA_NESTED}, {type : NLA_U32},
	    {type : NLA_FLAG},	 {type : NLA_NESTED},
	};

	struct nlattr *channeltb[NCSI_CHANNEL_ATTR_MAX + 1] = {nullptr};
	struct nla_policy channelPolicy[NCSI_CHANNEL_ATTR_MAX + 1] = {
	    {type : NLA_UNSPEC}, {type : NLA_NESTED}, {type : NLA_U32},
	    {type : NLA_FLAG},	 {type : NLA_NESTED}, {type : NLA_UNSPEC},
	};

	auto ret = genlmsg_parse(nlh, 0, tb, NCSI_ATTR_MAX, ncsiPolicy);
	if (!tb[NCSI_ATTR_PACKAGE_LIST]) {
		std::cerr << "No Packages" << std::endl;
		return -1;
	}

	auto attrTgt =
	    static_cast<nlattr *>(nla_data(tb[NCSI_ATTR_PACKAGE_LIST]));
	if (!attrTgt) {
		std::cerr << "Package list attribute is null" << std::endl;
		return -1;
	}

	auto rem = nla_len(tb[NCSI_ATTR_PACKAGE_LIST]);
	nla_for_each_nested(attrTgt, tb[NCSI_ATTR_PACKAGE_LIST], rem)
	{
		ret = nla_parse_nested(packagetb, NCSI_PKG_ATTR_MAX, attrTgt,
				       packagePolicy);
		if (ret < 0) {
			std::cerr << "Failed to parse package nested"
				  << std::endl;
			return -1;
		}

		if (packagetb[NCSI_PKG_ATTR_ID]) {
			auto attrID = nla_get_u32(packagetb[NCSI_PKG_ATTR_ID]);
			std::cout << "Package has id : " << std::hex << attrID
				  << std::endl;
		} else {
			std::cout << "Package with no id" << std::endl;
		}

		if (packagetb[NCSI_PKG_ATTR_FORCED]) {
			std::cout << "This package is forced" << std::endl;
		}

		auto channelListTarget = static_cast<nlattr *>(
		    nla_data(packagetb[NCSI_PKG_ATTR_CHANNEL_LIST]));

		auto channelrem =
		    nla_len(packagetb[NCSI_PKG_ATTR_CHANNEL_LIST]);
		nla_for_each_nested(channelListTarget,
				    packagetb[NCSI_PKG_ATTR_CHANNEL_LIST],
				    channelrem)
		{
			ret =
			    nla_parse_nested(channeltb, NCSI_CHANNEL_ATTR_MAX,
					     channelListTarget, channelPolicy);
			if (ret < 0) {
				std::cerr << "Failed to parse channel nested"
					  << std::endl;
				return -1;
			}

			if (channeltb[NCSI_CHANNEL_ATTR_ID]) {
				auto channel = nla_get_u32(
				    channeltb[NCSI_CHANNEL_ATTR_ID]);
				if (channeltb[NCSI_CHANNEL_ATTR_ACTIVE]) {
					std::cout
					    << "Channel Active : " << std::hex
					    << channel << std::endl;
				} else {
					std::cout << "Channel Not Active : "
						  << std::hex << channel
						  << std::endl;
				}

				if (channeltb[NCSI_CHANNEL_ATTR_FORCED]) {
					std::cout << "Channel is forced"
						  << std::endl;
				}
			} else {
				std::cout << "Channel with no ID" << std::endl;
			}

			if (channeltb[NCSI_CHANNEL_ATTR_VERSION_MAJOR]) {
				auto major = nla_get_u32(
				    channeltb[NCSI_CHANNEL_ATTR_VERSION_MAJOR]);
				std::cout
				    << "Channel Major Version : " << std::hex
				    << major << std::endl;
			}
			if (channeltb[NCSI_CHANNEL_ATTR_VERSION_MINOR]) {
				auto minor = nla_get_u32(
				    channeltb[NCSI_CHANNEL_ATTR_VERSION_MINOR]);
				std::cout
				    << "Channel Minor Version : " << std::hex
				    << minor << std::endl;
			}
			if (channeltb[NCSI_CHANNEL_ATTR_VERSION_STR]) {
				auto str = nla_get_string(
				    channeltb[NCSI_CHANNEL_ATTR_VERSION_STR]);
				std::cout << "Channel Version Str :" << str
					  << std::endl;
			}
			if (channeltb[NCSI_CHANNEL_ATTR_LINK_STATE]) {

				auto link = nla_get_u32(
				    channeltb[NCSI_CHANNEL_ATTR_LINK_STATE]);
				std::cout << "Channel Link State : " << std::hex
					  << link << std::endl;
			}
			if (channeltb[NCSI_CHANNEL_ATTR_VLAN_LIST]) {
				std::cout << "Active Vlan ids" << std::endl;
				auto vids =
				    channeltb[NCSI_CHANNEL_ATTR_VLAN_LIST];
				auto vid =
				    static_cast<nlattr *>(nla_data(vids));
				auto len = nla_len(vids);
				while (nla_ok(vid, len)) {
					auto id = nla_get_u16(vid);
					std::cout << "VID : " << id
						  << std::endl;
					vid = nla_next(vid, &len);
				}
			}
		}
	}
	return (int)NL_SKIP;
};

CallBack dataCallBack = [](struct nl_msg *msg, void *arg) {
	auto hdr = nlmsg_hdr(msg);
	struct nlattr *tb[NCSI_ATTR_MAX + 1] = {0};
	int rc;
	int *return_resp = (int *)arg;

	struct nla_policy ncsi_genl_policy[NCSI_ATTR_MAX + 1] = {
	    {type : NLA_UNSPEC}, {type : NLA_U32}, {type : NLA_NESTED},
	    {type : NLA_U32},	 {type : NLA_U32}, {type : NLA_BINARY},
	    {type : NLA_FLAG},	 {type : NLA_U32}, {type : NLA_U32}};

	rc = genlmsg_parse(hdr, 0, tb, NCSI_ATTR_MAX, ncsi_genl_policy);
	if (rc) {
		std::cerr << "Failed to parse ncsi cmd callback\n";
		return rc;
	}

	// data_len = nla_len(tb[NCSI_ATTR_DATA]) - ETHERNET_HEADER_SIZE;
	// data = (char*)(nla_data(tb[NCSI_ATTR_DATA])) + ETHERNET_HEADER_SIZE;

	ncsi_data_len = nla_len(tb[NCSI_ATTR_DATA]);
	ncsi_data = (uint8_t *)(nla_data(tb[NCSI_ATTR_DATA]));

	/*    resp_data.recv_len = nla_len(tb[NCSI_ATTR_DATA]);
	    memcpy(resp_data.recv_ncsi_data, (nla_data(tb[NCSI_ATTR_DATA])),
	   resp_data.recv_len); */

	/* std::cerr << "NC-SI Response Payload length = "<< ncsi_data_len
	 <<"\n"; std::cerr << "Response Payload:\n0: "; for (int i = 0; i <
	 ncsi_data_len; ++i)
	 {
	     if (i && !(i % 4))
		     {
				     std::cerr << "\n" << i <<": ";
		     }
	     printf("0x%02x ", *(ncsi_data + i));
	 }
	     std::cerr <<"\n";  */

	// indicating call back has been completed
	*return_resp = 0;
	return 0;
};

int applyCmd(int ifindex, int cmd, int package = DEFAULT_VALUE,
	     int channel = DEFAULT_VALUE, int flags = NONE,
	     CallBack function = nullptr, int opcode = DEFAULT_VALUE,
	     short payload_len = DEFAULT_VALUE, uint8_t *payload = nullptr)
{
	struct ncsi_pkt_hdr *hdr;
	uint8_t *pData, *pCtrlPktPayload;

	nlSocketPtr socket(nl_socket_alloc(), &::nl_socket_free);
	auto ret = genl_connect(socket.get());
	if (ret < 0) {
		std::cerr << "Failed to open the socket , RC : " << ret
			  << std::endl;
		return ret;
	}

	auto driverID = genl_ctrl_resolve(socket.get(), "NCSI");
	if (driverID < 0) {
		std::cerr << "Failed to resolve, RC : " << ret << std::endl;
		return driverID;
	}

	nlMsgPtr msg(nlmsg_alloc(), &::nlmsg_free);

	auto msgHdr = genlmsg_put(msg.get(), 0, 0, driverID, 0, flags, cmd, 0);
	if (!msgHdr) {
		std::cerr << "Unable to add the netlink headers , COMMAND : "
			  << cmd << std::endl;
		return -1;
	}

	if (package != DEFAULT_VALUE) {
		ret = nla_put_u32(msg.get(),
				  ncsi_nl_attrs::NCSI_ATTR_PACKAGE_ID, package);
		if (ret < 0) {
			std::cerr
			    << "Failed to set the attribute , RC : " << ret
			    << "PACKAGE " << std::hex << package << std::endl;
			return ret;
		}
	}

	if (channel != DEFAULT_VALUE) {
		ret = nla_put_u32(msg.get(),
				  ncsi_nl_attrs::NCSI_ATTR_CHANNEL_ID, channel);
		if (ret < 0) {
			std::cerr
			    << "Failed to set the attribute , RC : " << ret
			    << "CHANNEL : " << std::hex << channel << std::endl;
			return ret;
		}
	}

	ret = nla_put_u32(msg.get(), ncsi_nl_attrs::NCSI_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		std::cerr << "Failed to set the attribute , RC : " << ret
			  << "INTERFACE : " << std::hex << ifindex << std::endl;
		return ret;
	}

	if (opcode != DEFAULT_VALUE) {
		pData = (uint8_t *)calloc(1, sizeof(struct ncsi_pkt_hdr) +
						 payload_len);
		if (!pData) {
			std::cerr << "Failed to allocate buffer for ctrl pkt\n";
		}
		// prepare buffer to be copied to netlink msg
		hdr = (struct ncsi_pkt_hdr *)pData;
		pCtrlPktPayload = pData + sizeof(struct ncsi_pkt_hdr);
		memcpy(pCtrlPktPayload, payload, payload_len);
		hdr->type = opcode; // NC-SI command
		hdr->length =
		    htons(payload_len); // NC-SI command payload length

		ret = nla_put(msg.get(), NCSI_ATTR_DATA,
			      sizeof(struct ncsi_pkt_hdr) + payload_len,
			      (void *)pData);
		if (ret < 0) {
			std::cerr << "Failed to set the command "
				     "NCSI_CMD_SEND_CMD , RC : "
				  << ret << "\n";
			return ret;
		}

		nl_socket_disable_seq_check(socket.get());
		return_resp = 1;
	}

	if (function) {
		// Add a callback function to the socket
		nl_socket_modify_cb(socket.get(), NL_CB_VALID, NL_CB_CUSTOM,
				    function, &return_resp);
	}

	ret = nl_send_auto(socket.get(), msg.get());
	if (ret < 0) {
		std::cerr << "Failed to send the message , RC : " << ret
			  << std::endl;
		return ret;
	}

	if (opcode != DEFAULT_VALUE) {
		while (return_resp == 1) {
			ret = nl_recvmsgs_default(socket.get());
			if (ret < 0) {
				std::cerr
				    << "Failed to receive the message , RC : "
				    << ret << std::endl;
			}
		}
	} else {
		ret = nl_recvmsgs_default(socket.get());
		if (ret < 0) {
			std::cerr
			    << "Failed to receive the message , RC : " << ret
			    << std::endl;
		}
	}
	return ret;
}

} // namespace internal

int setChannel(int ifindex, int package, int channel)
{
	std::cout << "Set Channel : " << std::hex << channel
		  << ", PACKAGE : " << std::hex << package
		  << ", IFINDEX :  " << std::hex << ifindex << std::endl;
	return internal::applyCmd(ifindex,
				  ncsi_nl_commands::NCSI_CMD_SET_INTERFACE,
				  package, channel);
}

int sendCommand(int ifindex, int package, int channel, int opcode,
		short payload_len, uint8_t *payload)
{
	std::cout << "Send Command => Channel : " << std::hex << channel
		  << ", PACKAGE : " << std::hex << package
		  << ", IFINDEX :  " << std::hex << ifindex
		  << ", Opcode : " << std::hex << opcode
		  << ", Length : " << payload_len << std::endl;
	return internal::applyCmd(
	    ifindex, ncsi_nl_commands::NCSI_CMD_SEND_CMD, package, channel,
	    NONE, internal::dataCallBack, opcode, payload_len, payload);
}

int clearInterface(int ifindex)
{
	std::cout << "ClearInterface , IFINDEX :" << std::hex << ifindex
		  << std::endl;
	return internal::applyCmd(ifindex,
				  ncsi_nl_commands::NCSI_CMD_CLEAR_INTERFACE);
}

int getInfo(int ifindex, int package)
{
	std::cout << "Get Info , PACKAGE :  " << std::hex << package
		  << ", IFINDEX :  " << std::hex << ifindex << std::endl;
	if (package == DEFAULT_VALUE) {
		return internal::applyCmd(
		    ifindex, ncsi_nl_commands::NCSI_CMD_PKG_INFO, package,
		    DEFAULT_VALUE, NLM_F_DUMP, internal::infoCallBack);
	} else {
		return internal::applyCmd(
		    ifindex, ncsi_nl_commands::NCSI_CMD_PKG_INFO, package,
		    DEFAULT_VALUE, NONE, internal::infoCallBack);
	}
}

} // namespace ncsi
} // namespace network
} // namespace phosphor
