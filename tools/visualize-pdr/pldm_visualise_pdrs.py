#!/usr/bin/env python3

"""Tool to visualize PLDM pdrs"""

import argparse
import json
import hashlib
import sys
from datetime import datetime
import paramiko
from graphviz import Digraph
from tabulate import tabulate


def connect_to_bmc(hostname, uname, passwd, port):

    """ This function is responsible to connect to the BMC via
        ssh and returns a client object."""

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(hostname, username=uname, password=passwd, port=port)
    return client


def prepare_summary_report(state_sensor_pdr, state_effecter_pdr):

    """ This function is responsible to parse the state sensor pdr
        and the state effecter pdr dictionaries and creating the
        summary table."""

    summary_table = []
    headers = ["sensor_id", "entity_type", "state_set", "states"]
    summary_table.append(headers)
    for value in state_sensor_pdr.values():
        summary_record = []
        summary_record.extend([value["sensorID"], value["entityType"],
                               value["stateSetID[0]"],
                               value["possibleStates[0]"]])
        summary_table.append(summary_record)
    print("Created at : ", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
    print(tabulate(summary_table, tablefmt="fancy_grid", headers="firstrow"))

    summary_table = []
    headers = ["effecter_id", "entity_type", "state_set", "states"]
    summary_table.append(headers)
    for value in state_effecter_pdr.values():
        summary_record = []
        summary_record.extend([value["effecterID"], value["entityType"],
                               value["stateSetID[0]"],
                               value["possibleStates[0]"]])
        summary_table.append(summary_record)
    print(tabulate(summary_table, tablefmt="fancy_grid", headers="firstrow"))


def draw_entity_associations(pdr, counter):

    """ This function is responsible to create a picture that captures
        the entity association hierarchy based on the entity association
        PDR's received from the BMC."""

    dot = Digraph('entity_hierarchy', node_attr={'color': 'lightblue1',
                                                 'style': 'filled'})
    dot.attr(label=r'\n\nEntity Relation Diagram < ' +
             str(datetime.now().strftime("%Y-%m-%d %H:%M:%S"))+'>\n')
    dot.attr(fontsize='20')
    edge_list = []
    for value in pdr.values():
        parentnode = str(value["containerEntityType"]) + \
                     str(value["containerEntityInstanceNumber"])
        dot.node(hashlib.md5(parentnode.encode()).hexdigest(), parentnode)
        for i in range(1, value["containedEntityCount"]+1):
            childnode = str(value[f"containedEntityType[{i}]"]) + \
                        str(value[f"containedEntityInstanceNumber[{i}]"])
            dot.node(hashlib.md5(childnode.encode()).hexdigest(), childnode)
            if[hashlib.md5(parentnode.encode()).hexdigest(),
               hashlib.md5(childnode.encode()).hexdigest()] not in edge_list:
                edge_list.append([hashlib.md5(parentnode.encode()).hexdigest(),
                                  hashlib.md5(childnode.encode()).hexdigest()])
                dot.edge(hashlib.md5(parentnode.encode()).hexdigest(),
                         hashlib.md5(childnode.encode()).hexdigest())
    unflattentree = dot.unflatten(stagger=(round(counter/10)))
    unflattentree.render(filename='entity_association' +
                         str(datetime.now().strftime("%Y-%m-%d %H:%M:%S")),
                         view=False, cleanup=True, format='pdf')


def fetch_pdrs_from_bmc(client):

    """ This is the core function that would use the existing ssh connection
        object to connect to BMC and fire the getPDR pldmtool command
        and it then agreegates the data received from all the calls into
        the respective disctionaries based on the PDR Type."""

    entity_association_pdr = {}
    state_sensor_pdr = {}
    state_effecter_pdr = {}
    state_effecter_pdr = {}
    numeric_pdr = {}
    fru_record_set_pdr = {}
    tl_pdr = {}
    for i in range(1, 2**32):
        my_str = ''
        command = 'pldmtool platform getpdr -d' + str(i)
        output = client.exec_command(command)
        for line in output[1]:
            my_str += line.strip('\n')
        my_dic = json.loads(my_str)
        sys.stdout.write("Fetching PDR's from BMC : %d \r" % (i))
        sys.stdout.flush()
        if my_dic["PDRType"] == "Entity Association PDR":
            entity_association_pdr[i] = my_dic
        if my_dic["PDRType"] == "State Sensor PDR":
            state_sensor_pdr[i] = my_dic
        if my_dic["PDRType"] == "State Effecter PDR":
            state_effecter_pdr[i] = my_dic
        if my_dic["PDRType"] == "FRU Record Set PDR":
            fru_record_set_pdr[i] = my_dic
        if my_dic["PDRType"] == "Terminus Locator PDR":
            tl_pdr[i] = my_dic
        if my_dic["PDRType"] == "Numeric Effecter PDR":
            numeric_pdr[i] = my_dic
        if my_dic["nextRecordHandle"] == 0:
            break
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
                        help="BMC Ip address/BMC Hostname")
    parser.add_argument('--uname', type=str, required=True,
                        help="BMC username")
    parser.add_argument('--passwd', type=str, required=True,
                        help="BMC Password")
    parser.add_argument('--port', type=int, help="BMC SSH port",
                        default=22)
    args = parser.parse_args()
    if args.bmc and args.passwd and args.uname:
        client = connect_to_bmc(args.bmc, args.uname, args.passwd, args.port)
        association_pdr, state_sensor_pdr, state_effecter_pdr, counter = \
            fetch_pdrs_from_bmc(client)
        draw_entity_associations(association_pdr, counter)
        prepare_summary_report(state_sensor_pdr, state_effecter_pdr)


if __name__ == "__main__":
    main()
