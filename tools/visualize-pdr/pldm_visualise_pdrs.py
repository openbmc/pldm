import argparse
import json
import hashlib
import multiprocessing
import sys
from datetime import datetime
import paramiko
from graphviz import Digraph
from tabulate import tabulate
from ctypes import c_char_p


def fetch_pdr(handle_number,entity_association_pdr, state_sensor_pdr,state_effecter_pdr,fail,hostname,uname,passwd,port,counter):
    if fail.value <= 2 :
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        client.connect(hostname.value, username=uname.value, password=passwd.value,port=port.value)
        my_str = ''
        command = 'pldmtool platform getpdr -d'+ str(handle_number)
        output  = client.exec_command(command)
        for line in output[1]:
            my_str += line.strip('\n')
        try:
            my_dic = json.loads(my_str)
            counter.value += 1
            print("#",end="")
            sys.stdout.flush()
            if my_dic["PDRType"] == "Entity Association PDR":
                entity_association_pdr[handle_number]= my_dic
            if my_dic["PDRType"] == "State Sensor PDR":
                state_sensor_pdr[handle_number] = my_dic
            if my_dic["PDRType"] == "State Effecter PDR":
                state_effecter_pdr[handle_number] = my_dic
            client.close()
        except ValueError as e:
            fail.value +=1
            client.close()
    else:
        fail +=1
        return

def prepare_summary_report(state_sensor_pdr,state_effecter_pdr):
    summary_table = []
    headers = ["sensor_id", "entity_type", "state_set", "states"]
    summary_table.append(headers)
    for value in state_sensor_pdr.values():
        summary_record = []
        summary_record.extend([value["sensorID"],value["entityType"], value["stateSetID[0]"],value["possibleStates[0]"]])
        summary_table.append(summary_record)
    print("Summary Created at : ", datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
    print(tabulate(summary_table,tablefmt="fancy_grid",headers="firstrow"))

    summary_table = []
    headers = ["effecter_id", "entity_type", "state_set", "states"]
    summary_table.append(headers)
    for value in state_effecter_pdr.values():
        summary_record = []
        summary_record.extend([value["effecterID"],value["entityType"],value["stateSetID[0]"],value["possibleStates[0]"]])
        summary_table.append(summary_record)
    print(tabulate(summary_table,tablefmt="fancy_grid",headers="firstrow"))

def draw_entity_associations(pdr,counter):
    dot = Digraph('entity_association_hierarchy',node_attr={'color': 'lightblue1', 'style': 'filled'})
    dot.attr(label=r'\n\nEntity Relation Diagram < '+str(datetime.now().strftime("%Y-%m-%d %H:%M:%S"))+'>\n')
    dot.attr(fontsize='20')

    for value in pdr.values():
        #print(json.dumps(value, indent=4, sort_keys=True))
        parentnode = str(value["containerEntityType"]) + str(value["containerEntityInstanceNumber"])
        dot.node(hashlib.md5(parentnode.encode()).hexdigest(), parentnode)
        #print("parent : ", parentnode)
        for i in range(1,value["containedEntityCount"]+1):
            childnode = str(value[f"containedEntityType[{i}]"])+ str(value[f"containedEntityInstanceNumber[{i}]"])
            dot.node(hashlib.md5(childnode.encode()).hexdigest(),childnode)
            dot.edge(hashlib.md5(parentnode.encode()).hexdigest(),hashlib.md5(childnode.encode()).hexdigest())
    #print(dot.source)
    print(counter.value/10)
    unflattentree = dot.unflatten(stagger=(counter.value/10))
    unflattentree.render(filename='entity_association'+str(datetime.now().strftime("%Y-%m-%d %H:%M:%S")), view=True, cleanup=True, format='pdf')

def fork_process_for_pdr(hostname,uname,passwd,port):
    manager = multiprocessing.Manager()
    entity_association_pdr = manager.dict()
    state_sensor_pdr = manager.dict()
    state_effecter_pdr = manager.dict()
    hostname = manager.Value(c_char_p, hostname)
    uname = manager.Value(c_char_p, uname)
    passwd = manager.Value(c_char_p,passwd)
    port = manager.Value("i",port)
    fail_count = manager.Value('i',0)
    counter = manager.Value("i",0)
    #print(fail_count.value)
    pool = multiprocessing.Pool(processes=8)
    #print("after pool")
    for i in range(100):
        pool.apply_async(fetch_pdr,args=(i,entity_association_pdr,state_sensor_pdr,state_effecter_pdr,fail_count,hostname,uname,passwd,port,counter))
    pool.close()
    pool.join()
    print("|\n"+"Successfully fetched "+str(counter.value)+" PDR\'s")
    return entity_association_pdr,state_sensor_pdr,state_effecter_pdr,counter

def main():
    parser = argparse.ArgumentParser(prog='pldmvisualise')
    parser.add_argument('--bmc', type=str, required=True, help="BMC Ip address/BMC Hostname")
    parser.add_argument('--uname',type=str,required=True, help="BMC username")
    parser.add_argument('--passwd',type=str, required=True, help="BMC Password")
    parser.add_argument('--port',type=int,help="BMC SSH port",default=22)
    args = parser.parse_args()
    if args.bmc and args.passwd and args.uname:
        print('Fetching PDR\'s |',end="")
        entity_association_pdr, state_sensor_pdr, state_effecter_pdr,counter  = fork_process_for_pdr(args.bmc,args.uname, args.passwd, args.port)

        draw_entity_associations(entity_association_pdr,counter)
        prepare_summary_report(state_sensor_pdr, state_effecter_pdr)

if __name__ == "__main__":
    main()
