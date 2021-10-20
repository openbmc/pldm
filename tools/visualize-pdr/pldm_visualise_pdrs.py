#!/usr/bin/env python3

"""Tool to visualize PLDM PDR's"""

import argparse
import json
import hashlib
import sys
from datetime import datetime
import paramiko
from graphviz import Digraph
from tabulate import tabulate
import os


def connect_to_bmc(hostname, uname, passwd, port, **kw):

    """ This function is responsible to connect to the BMC via
        ssh and returns a client object.

        Parameters:
            hostname: hostname/IP address of BMC
            uname: ssh username of BMC
            passwd: ssh password of BMC
            port: ssh port of BMC

    """

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(hostname, username=uname, password=passwd, port=port, **kw)
    return client


def prepare_summary_report(state_sensor_pdr, state_effecter_pdr):

    """ This function is responsible to parse the state sensor pdr
        and the state effecter pdr dictionaries and creating the
        summary table.

        Parameters:
            state_sensor_pdr: list of state sensor pdrs
            state_effecter_pdr: list of state effecter pdrs

    """

    summary_table = []
    headers = ["sensor_id", "entity_type", "state_set", "states"]
    summary_table.append(headers)
    for value in state_sensor_pdr.values():
        summary_record = []
        sensor_possible_states = ''
        for sensor_state in value["possibleStates[0]"]:
            sensor_possible_states += sensor_state+"\n"
        summary_record.extend([value["sensorID"], value["entityType"],
                               value["stateSetID[0]"],
                               sensor_possible_states])
        summary_table.append(summary_record)
    print("Created at : ", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
    print(tabulate(summary_table, tablefmt="fancy_grid", headers="firstrow"))

    summary_table = []
    headers = ["effecter_id", "entity_type", "state_set", "states"]
    summary_table.append(headers)
    for value in state_effecter_pdr.values():
        summary_record = []
        effecter_possible_states = ''
        for state in value["possibleStates[0]"]:
            effecter_possible_states += state+"\n"
        summary_record.extend([value["effecterID"], value["entityType"],
                               value["stateSetID[0]"],
                               effecter_possible_states])
        summary_table.append(summary_record)
    print(tabulate(summary_table, tablefmt="fancy_grid", headers="firstrow"))


def draw_entity_associations(pdr, counter):

    """ This function is responsible to create a picture that captures
        the entity association hierarchy based on the entity association
        PDR's received from the BMC.

        Parameters:
            pdr: list of entity association PDR's
            counter: variable to capture the count of PDR's to unflatten
                     the tree

    """

    dot = Digraph('entity_hierarchy', node_attr={'color': 'lightblue1',
                                                 'style': 'filled'})
    dot.attr(label=r'\n\nEntity Relation Diagram < ' +
             str(datetime.now().strftime("%Y-%m-%d %H:%M:%S"))+'>\n')
    dot.attr(fontsize='20')
    edge_list = []
    for value in pdr.values():
        parentnode = str(value["containerEntityType"]) + \
                     str(value["containerEntityInstanceNumber"])
        dot.node(hashlib.md5((parentnode +
                              str(value["containerEntityContainerID"]))
                             .encode()).hexdigest(), parentnode)

        for i in range(1, value["containedEntityCount"]+1):
            childnode = str(value[f"containedEntityType[{i}]"]) + \
                        str(value[f"containedEntityInstanceNumber[{i}]"])
            cid = str(value[f"containedEntityContainerID[{i}]"])
            dot.node(hashlib.md5((childnode + cid)
                                 .encode()).hexdigest(), childnode)

            if[hashlib.md5((parentnode +
                            str(value["containerEntityContainerID"]))
                           .encode()).hexdigest(),
               hashlib.md5((childnode + cid)
                           .encode()).hexdigest()] not in edge_list:
                edge_list.append([hashlib.md5((parentnode +
                                  str(value["containerEntityContainerID"]))
                                              .encode()).hexdigest(),
                                  hashlib.md5((childnode + cid)
                                              .encode()).hexdigest()])
                dot.edge(hashlib.md5((parentnode +
                                      str(value["containerEntityContainerID"]))
                                     .encode()).hexdigest(),
                         hashlib.md5((childnode + cid).encode()).hexdigest())
    unflattentree = dot.unflatten(stagger=(round(counter/3)))
    unflattentree.render(filename='entity_association_' +
                         str(datetime.now().strftime("%Y-%m-%d_%H-%M-%S")),
                         view=False, cleanup=True, format='pdf')


class PLDMToolError(Exception):
    """ Exception class intended to be used to hold pldmtool invocation failure
        information such as exit status and stderr.

    """

    def __init__(self, status, stderr):
        msg = "pldmtool failed with exit status {}.\n".format(status)
        msg += "stderr: \n\n{}".format(stderr)
        super(PLDMToolError, self).__init__(msg)
        self.status = status

    def get_status(self):
        return self.status


def process_pldmtool_output(stdout_channel, stderr_channel):
    """ Ensure pldmtool runs without error and if it does fail, detect that and
        show the pldmtool exit status and it's stderr.

        A simpler implementation would just wait for the pldmtool exit status
        prior to attempting to decode it's stdout.  Instead, optimize for the
        no error case and allow the json decoder to consume pldmtool stdout as
        soon as it is available (in parallel).  This results in the following
        error scenarios:
            - pldmtool fails and the decoder fails
              Ignore the decoder fail and throw PLDMToolError.
            - pldmtool fails and the decoder doesn't fail
              Throw PLDMToolError.
            - pldmtool doesn't fail and the decoder does fail
              This is a pldmtool bug - re-throw the decoder error.

        Parameters:
            stdout_channel: file-like stdout channel
            stderr_channel: file-like stderr channel

    """

    status = 0
    try:
        data = json.load(stdout_channel)
        # it's unlikely, but possible, that pldmtool failed but still wrote a
        # valid json document - so check for that.
        status = stderr_channel.channel.recv_exit_status()
        if status == 0:
            return data
    except json.decoder.JSONDecodeError:
        # pldmtool wrote an invalid json document.  Check to see if it had
        # non-zero exit status.
        status = stderr_channel.channel.recv_exit_status()
        if status == 0:
            # pldmtool didn't have non zero exit status, so it wrote an invalid
            # json document and the JSONDecodeError is the correct error.
            raise

    # pldmtool had a non-zero exit status, so throw an error for that, possibly
    # discarding a spurious JSONDecodeError exception.
    raise PLDMToolError(status, "".join(stderr_channel))


def get_pdrs_one_at_a_time(client):
    """ Using pldmtool over SSH, generate (record handle, PDR) tuples for each
        record in the PDR repository.

        Parameters:
            client: paramiko ssh client object

    """

    command_fmt = 'pldmtool platform getpdr -d {}'
    record_handle = 0
    while True:
        output = client.exec_command(command_fmt.format(str(record_handle)))
        _, stdout, stderr = output
        pdr = process_pldmtool_output(stdout, stderr)
        yield record_handle, pdr
        record_handle = pdr["nextRecordHandle"]
        if record_handle == 0:
            break


def get_all_pdrs_at_once(client):
    """ Using pldmtool over SSH, generate (record handle, PDR) tuples for each
        record in the PDR repository.  Use pldmtool platform getpdr --all.

        Parameters:
            client: paramiko ssh client object

    """

    _, stdout, stderr = client.exec_command('pldmtool platform getpdr -a')
    all_pdrs = process_pldmtool_output(stdout, stderr)

    # Explicitly request record 0 to find out what the real first record is.
    _, stdout, stderr = client.exec_command('pldmtool platform getpdr -d 0')
    pdr_0 = process_pldmtool_output(stdout, stderr)
    record_handle = pdr_0["recordHandle"]

    while True:
        for pdr in all_pdrs:
            if pdr["recordHandle"] == record_handle:
                yield record_handle, pdr
                record_handle = pdr["nextRecordHandle"]
                if record_handle == 0:
                    return
        raise RuntimeError(
            "Dangling reference to record {}".format(record_handle))


def get_pdrs(client):
    """ Using pldmtool over SSH, generate (record handle, PDR) tuples for each
        record in the PDR repository.  Use pldmtool platform getpdr --all or
        fallback on getting them one at a time if pldmtool doesn't support the
        --all option.

        Parameters:
            client: paramiko ssh client object

    """
    try:
        for record_handle, pdr in get_all_pdrs_at_once(client):
            yield record_handle, pdr
        return
    except PLDMToolError as e:
        # No support for the -a option
        if e.get_status() != 106:
            raise
    except json.decoder.JSONDecodeError as e:
        # Some versions of pldmtool don't print valid json documents with -a
        if e.msg != "Extra data":
            raise

    for record_handle, pdr in get_pdrs_one_at_a_time(client):
        yield record_handle, pdr


def fetch_pdrs_from_bmc(client):

    """ This is the core function that would use the existing ssh connection
        object to connect to BMC and fire the getPDR pldmtool command
        and it then agreegates the data received from all the calls into
        the respective dictionaries based on the PDR Type.

        Parameters:
            client: paramiko ssh client object

    """

    entity_association_pdr = {}
    state_sensor_pdr = {}
    state_effecter_pdr = {}
    state_effecter_pdr = {}
    numeric_pdr = {}
    fru_record_set_pdr = {}
    tl_pdr = {}
    for handle_number, my_dic in get_pdrs(client):
        sys.stdout.write("Fetching PDR's from BMC : %8d\r" % (handle_number))
        sys.stdout.flush()
        if my_dic["PDRType"] == "Entity Association PDR":
            entity_association_pdr[handle_number] = my_dic
        if my_dic["PDRType"] == "State Sensor PDR":
            state_sensor_pdr[handle_number] = my_dic
        if my_dic["PDRType"] == "State Effecter PDR":
            state_effecter_pdr[handle_number] = my_dic
        if my_dic["PDRType"] == "FRU Record Set PDR":
            fru_record_set_pdr[handle_number] = my_dic
        if my_dic["PDRType"] == "Terminus Locator PDR":
            tl_pdr[handle_number] = my_dic
        if my_dic["PDRType"] == "Numeric Effecter PDR":
            numeric_pdr[handle_number] = my_dic
    client.close()

    total_pdrs = len(entity_association_pdr.keys()) + len(tl_pdr.keys()) + \
        len(state_effecter_pdr.keys()) + len(numeric_pdr.keys()) + \
        len(state_sensor_pdr.keys()) + len(fru_record_set_pdr.keys())
    print("\nSuccessfully fetched " + str(total_pdrs) + " PDR\'s")
    print("Number of FRU Record PDR's : ", len(fru_record_set_pdr.keys()))
    print("Number of TerminusLocator PDR's : ", len(tl_pdr.keys()))
    print("Number of State Sensor PDR's : ", len(state_sensor_pdr.keys()))
    print("Number of State Effecter PDR's : ", len(state_effecter_pdr.keys()))
    print("Number of Numeric Effecter PDR's : ", len(numeric_pdr.keys()))
    print("Number of Entity Association PDR's : ",
          len(entity_association_pdr.keys()))
    return (entity_association_pdr, state_sensor_pdr,
            state_effecter_pdr, len(fru_record_set_pdr.keys()))


def main():

    """ Create a summary table capturing the information of all the PDR's
        from the BMC & also create a diagram that captures the entity
        association hierarchy."""

    parser = argparse.ArgumentParser(prog='pldm_visualise_pdrs.py')
    parser.add_argument('--bmc', type=str, required=True,
                        help="BMC IPAddress/BMC Hostname")
    parser.add_argument('--user', type=str, help="BMC username")
    parser.add_argument('--password', type=str, required=True,
                        help="BMC Password")
    parser.add_argument('--port', type=int, help="BMC SSH port",
                        default=22)
    args = parser.parse_args()

    extra_cfg = {}
    try:
        with open(os.path.expanduser("~/.ssh/config")) as f:
            ssh_config = paramiko.SSHConfig()
            ssh_config.parse(f)
            host_config = ssh_config.lookup(args.bmc)
            if host_config:
                if 'hostname' in host_config:
                    args.bmc = host_config['hostname']
                if 'user' in host_config and args.user is None:
                    args.user = host_config['user']
                if 'proxycommand' in host_config:
                    extra_cfg['sock'] = paramiko.ProxyCommand(
                        host_config['proxycommand'])
    except FileNotFoundError:
        pass

    client = connect_to_bmc(
        args.bmc, args.user, args.password, args.port, **extra_cfg)
    association_pdr, state_sensor_pdr, state_effecter_pdr, counter = \
        fetch_pdrs_from_bmc(client)
    draw_entity_associations(association_pdr, counter)
    prepare_summary_report(state_sensor_pdr, state_effecter_pdr)


if __name__ == "__main__":
    main()
