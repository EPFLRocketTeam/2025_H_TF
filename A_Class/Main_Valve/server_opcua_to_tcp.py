# OPC UA to Valve TCP Bridge (Windows, full dual-valve support)
# Connects to WAGO OPC UA server and relays variables to each valve (Ethanol & N2O) over TCP


# ==== Imports ====
import socket
import json
import base64
import time
from opcua import Client, ua


# === OPC UA Setup ===
OPCUA_SERVER_URL = "opc.tcp://192.168.1.17:4840"
client = Client(OPCUA_SERVER_URL)
client.set_user("")
client.set_password("")
def connect_opcua():
    print("Trying to connect OPCUA...")
    try:
        client.connect()
        print("Connected to OPC UA valve system")
        return True
    except Exception as e:
        print(f"Failed to connect OPCUA: {e}")
        try:
            client.disconnect()
        except:
            pass
        time.sleep(3)
        return False
    

# === NodeId declarations ===
node_b_Homing_E      = ua.NodeId(base64.b64decode("AQAAAKbhKnGK9zM6o+Y1NI3mYGeQ7iJ7heYzOovcCHuE6i5ztsZA"), 5, ua.NodeIdType.ByteString)
node_b_SingleStep_E  = ua.NodeId(base64.b64decode("AQAAAKbhKnGK9zM6o+Y1NI3mYGeQ7iJ7heYzOovcE32H5CxxuvclZLbGQA=="), 5, ua.NodeIdType.ByteString)
node_i_status_epos_E = ua.NodeId(base64.b64decode("AQAAAKbhKnGK9zM6o+Y1NI3mYGeQ7iJ7heYzOoDcE2CI9zVntuYwe5rcBRQ="), 5, ua.NodeIdType.ByteString)
node_w_main_E        = ua.NodeId(base64.b64decode("AQAAAKbhKnGK9zM6o+Y1NI3mYGeQ7iJ7heYzOp7cDXWA7QVCtsZA"), 5, ua.NodeIdType.ByteString)
node_b_Homing_O      = ua.NodeId(base64.b64decode("AQAAAKbhKnGK9zM6o+Y1NI3mYGeQ7iJ7heYzOovcCHuE6i5ztsxA"), 5, ua.NodeIdType.ByteString)
node_b_SingleStep_O  = ua.NodeId(base64.b64decode("AQAAAKbhKnGK9zM6o+Y1NI3mYGeQ7iJ7heYzOovcE32H5CxxuvclZLbMQA=="), 5, ua.NodeIdType.ByteString)
node_i_status_epos_O = ua.NodeId(base64.b64decode("AQAAAKbhKnGK9zM6o+Y1NI3mYGeQ7iJ7heYzOoDcE2CI9zVntuYwe5rcDxQ="), 5, ua.NodeIdType.ByteString)
node_w_main_O        = ua.NodeId(base64.b64decode("AQAAAKbhKnGK9zM6o+Y1NI3mYGeQ7iJ7heYzOp7cDXWA7QVCtsxA"), 5, ua.NodeIdType.ByteString)


# === TCP server setup ===
HOST = "0.0.0.0"
PORT_ETH = 4850
PORT_N2O = 4851
sock_eth = None
sock_n2o = None
ethanol_conn = None
n2o_conn = None
homing_e = 0
homing_o = 0
main_e   = 0
main_o   = 0
single_e = 0
single_o = 0
def connect_tcp():
    global ethanol_conn, sock_eth, n2o_conn, sock_n2o
    sock_eth = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock_eth.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock_n2o = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock_n2o.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        # Bind and listen for Ethanol valve
        sock_eth.bind((HOST, PORT_ETH))
        sock_eth.listen(1)
        print(f"Valve system server running on port {PORT_ETH}")
        print("Waiting for Ethanol valve to connect...")
        ethanol_conn, addr = sock_eth.accept()
        print(f"Ethanol valve connected from {addr}")

        # Bind and listen for N2O valve
        sock_n2o.bind((HOST, PORT_N2O))
        sock_n2o.listen(1)
        print(f"Valve system server running on port {PORT_N2O}")
        print("Waiting for N2O valve to connect...")
        n2o_conn, addr = sock_n2o.accept()
        print(f"N2O valve connected from {addr}")

        return True
    
    except Exception as e:
        print(f"Failed to connect TCP: {e}")
        try:
            ethanol_conn.close()
        except:
            pass
        try:
            sock_eth.close()
        except:
            pass
        time.sleep(1)
        return False

def read_opcua():
    global homing_e, homing_o, main_e, main_o, single_e, single_o
    try:
        homing_e = client.get_node(node_b_Homing_E).get_value()
        homing_o = client.get_node(node_b_Homing_O).get_value()
        main_e   = client.get_node(node_w_main_E).get_value()
        main_o   = client.get_node(node_w_main_O).get_value()
        single_e = client.get_node(node_b_SingleStep_E).get_value()
        single_o = client.get_node(node_b_SingleStep_O).get_value()
        return True
    except Exception as e:
        print(f"OPC UA read failed: {e}")
        homing_e = homing_o = single_e = single_o = 0
        main_e = main_o = 0
        return False

def send_tcp():
    global ethanol_conn, sock_eth, homing_e, homing_o, main_e, main_o, single_e, single_o
    data_e = json.dumps({   "b_Homing_E": homing_e,
                            "w_Main_EV": main_e,
                            "b_SingleStep_E": single_e
                        }) + "\n"
    data_o = json.dumps({   "b_Homing_O": homing_o,
                            "w_Main_OV": main_o,
                            "b_SingleStep_O": single_o
                        }) + "\n"
    try:
        ethanol_conn.sendall(data_e.encode())
        n2o_conn.sendall(data_o.encode())
        return True
        
    except Exception as e:
        print(f"TCP send error: {e}")
        return False
    
def print_separation():
    print("\n"*3,"-"*40)

def loop():
    # Read OPCUA
    if(not read_opcua()):
        connect_opcua()
    # Send TCP
    if(not send_tcp()):
        connect_tcp()
    print(f"homing_e: {homing_e}")

def main():
    try:
        # Connection OPCUA
        while(not connect_opcua()):
            pass
        # Connection TCP
        while(not connect_tcp()):
            pass
        # Loop
        while(True):
            loop()
    except:
        # Shutting down
        print("Shutting down server...")
        try:
            ethanol_conn.close()
        except:
            pass
        try:
            sock_eth.close()
        except:
            pass
        try:
            client.disconnect()
        except:
            pass
        print("Server shut down")

# ==== Main ====
if __name__ == "__main__":
    main()
    exit()










