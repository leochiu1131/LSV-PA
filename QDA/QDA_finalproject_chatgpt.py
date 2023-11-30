from qiskit import *
from numpy import pi
from matplotlib import pyplot as plt
import numpy

theta_list=[]
probability_0_list=[]
probability_1_list=[]
backend = Aer.get_backend('qasm_simulator')

def circuit_test(n,theta):
    
    circuit = QuantumCircuit(1,1)
    circuit.h(0)
    
    for i in numpy.arange( 0 , theta , theta/n) :
        circuit.p(i , 0)
        circuit.h(0)
        circuit.measure([0],[0])
        print(circuit.draw())
        job_sim = execute(circuit, backend, shots=1024)
        sim_result = job_sim.result()
        counts = sim_result.get_counts()
        
        probability_0 = counts.get('0', 0)/1024
        probability_1 = counts.get('1', 0)/1024
        
        print("counts['0']=", counts.get('0', 0))
        print("counts['1']=", counts.get('1', 0))
    
        probability_0_list.append(probability_0)
        probability_1_list.append(probability_1)
        theta_list.append(i)

print(circuit_test(25,2*pi))

plt.plot(theta_list, probability_0_list,'ro',
         theta_list, probability_1_list,'bo')
plt.xlabel("theta", fontsize=13)
plt.ylabel("probability", fontsize=13)
plt.show()