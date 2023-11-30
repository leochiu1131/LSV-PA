# -*- coding: utf-8 -*-
import time
import networkx as nx
import pennylane as qml
from pennylane import numpy as np
from argparse import ArgumentParser

def read_parse():
    parser = ArgumentParser(description = "[HomeWork] QAOA experiment for the unweighted k-regular graph")
    parser.add_argument("-n", "--num_nodes", default=12,
                        type=int, help="number of nodes")
    parser.add_argument("-s", "--seed", default=1234,
                        type=int, help="random seed (Do Not Change This Random Seed)")
    parser.add_argument("-k", "--k_regular", default=3,
                        type=int, help="number of the regular")
    parser.add_argument("-mp", "--max_p", default=5,
                        type=int, help="The maximum p-level (depth)")
    parser.add_argument("-ts", "--total_step", default=10,
                        type=int, help="The optimization step")
    return parser.parse_args()

def get_cost(b, num_nodes, w):
    x = [(b >> (num_nodes-1-i)) & 1 for i in range(num_nodes)]
    cost = 0
    for i in range(num_nodes):
        for j in range(num_nodes):
            cost = cost + w[i,j] * x[i] * (1-x[j])
    return cost

# unitary operator U_B with parameter beta
def U_B(beta, n_wires):
    for wire in range(n_wires):
        qml.RX(2 * beta, wires=wire)

# unitary operator U_C with parameter gamma
def U_C(gamma, graph):
    for edge in graph:
        wire1 = edge[0]
        wire2 = edge[1]
        qml.CNOT(wires=[wire1, wire2])
        qml.RZ(gamma, wires=wire2)
        qml.CNOT(wires=[wire1, wire2])

def bitstring_to_int(bit_string_sample):
    bit_string = "".join(str(bs) for bs in bit_string_sample)
    return int(bit_string, base=2)

def qaoa_maxcut(args = None, n_layers = 1, graph = None, w = None):
    if(not args):
        return
    # prepare variables
    n_wires  = args.num_nodes
    step     = args.total_step
    np.random.seed(args.seed)

    # the qaoa circuit
    dev = qml.device("lightning.qubit", wires=n_wires, shots=1)
    @qml.qnode(dev)
    def circuit(gammas, betas, edge=None, n_layers=1, graph = None, n_wires = None):
        # apply Hadamards to get the n qubit |+> state
        for wire in range(n_wires):
            qml.Hadamard(wires=wire)
        # p instances of unitary operators
        for i in range(n_layers):
            U_C(gammas[i], graph)
            U_B(betas[i], n_wires=n_wires)
        if edge is None:
            # measurement phase
            return qml.sample()
        # during the optimization phase we are evaluating a term
        # in the objective using expval
        H = qml.PauliZ(edge[0]) @ qml.PauliZ(edge[1])
        return qml.expval(H)

    # minimize the negative of the objective function
    def objective(params):
        gammas = params[0]
        betas = params[1]
        neg_obj = 0
        for edge in graph:
            # objective for the MaxCut problem
            neg_obj -= 0.5 * (1 - circuit(gammas, betas, edge=edge, n_layers=n_layers, graph = graph, n_wires = n_wires))
        return neg_obj
    
    # initialize the parameters near zero
    init_params = 0.01 * np.ones((2, n_layers), requires_grad=True)

    # initialize optimizer: Adagrad works well empirically
    opt = qml.AdagradOptimizer(stepsize=0.5)

    # optimize parameters in objective
    params = init_params
    for i in range(step):
        params = opt.step(objective, params)

    # sample measured bitstrings 100 times
    bit_strings = []
    n_samples = 100
    for i in range(0, n_samples):
        bit_strings.append(bitstring_to_int(circuit(params[0], params[1], edge=None, n_layers=n_layers, graph = graph, n_wires = n_wires)))
    # print optimal parameters and most frequently sampled bitstring
    counts = np.bincount(np.array(bit_strings))
    most_freq_bit_string = np.argmax(counts)
    print("Cost of most frequently sampled bit string is: {}".format(get_cost(most_freq_bit_string, num_nodes=n_wires, w=w)))
    return -objective(params), bit_strings

def main():
    args = read_parse()
    print(args)

    # create unweighted k-regular graph
    n_wires = args.num_nodes
    G = nx.random_regular_graph(args.k_regular, n_wires, seed=args.seed)
    graph = G.edges
    w = nx.adjacency_matrix(G, nodelist=range(n_wires)).todense()

    # *** [Homework Start] *** 
    for i in range(1, args.max_p + 1):
        print("\n ********** p={} **********".format(i))
        start_time = time.time()
        qaoa_maxcut(args, n_layers = i, graph = graph, w = w) #hw
        print("{:.2f} seconds".format(time.time() - start_time))

if __name__ == '__main__':
    main()
