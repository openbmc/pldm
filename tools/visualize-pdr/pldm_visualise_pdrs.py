#!/usr/bin/env python3

"""Tool to visualize PLDM pdrs"""

import argparse
import json
import hashlib
import multiprocessing
from multiprocessing import set_start_method
from multiprocessing import get_context
import sys
from datetime import datetime
from ctypes import c_char_p
import paramiko
from graphviz import Digraph
from tabulate import tabulate


def fetch_pdr(handle_number, entity_association_pdr, state_sensor_pdr,
              state_effecter_pdr, numeric_pdr, fru_record_set_pdr, tl_pdr,
              hostname, uname, passwd, port, fail):

    """ This function is responsible to fetching the pdr correponding to
        the handle_number & places the PDR's based on thier type to the
        respective containers."""
    if fail.value <= 5:
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        client.connect(hostname.value, username=uname.value,
                       password=passwd.value, port=port.value)
        my_str = ''
        command = 'pldmtool platform getpdr -d' + str(handle_number)
        output = client.exec_command(command)
        for line in output[1]:
            my_str += line.strip('\n')
        try:
            my_dic = json.loads(my_str)
            sys.stdout.write("Fetching PDR's in Parallel : %d \r"
                             % (handle_number))
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
            if my_dic["nextRecordHandle"] == 0:
                client.close()
                fail.value += 1
            client.close()
        except ValueError:
            client.close()
            fail.value += 1
    else:
        fail.value += 1
        return


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


def fork_process_for_pdr(hostname, uname, passwd, port):

    """ This is the core function that spawns multiple forks and each fork
        would then connect to BMC and fire the getPDR pldmtool command
        and it then agreegates the data received from all the forks into
        the respective disctionaries based on the PDR Type."""

    set_start_method("spawn")
    manager = multiprocessing.Manager()
    entity_association_pdr = manager.dict()
    state_sensor_pdr = manager.dict()
    state_effecter_pdr = manager.dict()
    numeric_pdr = manager.dict()
    fru_record_set_pdr = manager.dict()
    tl_pdr = manager.dict()
    hostname = manager.Value(c_char_p, hostname)
    uname = manager.Value(c_char_p, uname)
    passwd = manager.Value(c_char_p, passwd)
    port = manager.Value("i", port)
    fail = manager.Value("i", 0)
    with get_context("spawn").Pool() as pool:
        for i in range(1, 1000):
            pool.apply_async(fetch_pdr, args=(i, entity_association_pdr,
                             state_sensor_pdr, state_effecter_pdr, numeric_pdr,
                             fru_record_set_pdr, tl_pdr, hostname, uname,
                             passwd, port, fail))
        pool.close()
        pool.join()
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
        association_pdr, state_sensor_pdr, state_effecter_pdr, counter = \
            fork_process_for_pdr(args.bmc, args.uname, args.passwd, args.port)
        draw_entity_associations(association_pdr, counter)
        prepare_summary_report(state_sensor_pdr, state_effecter_pdr)


if __name__ == "__main__":
    main()
