
from qiskit import QuantumRegister, ClassicalRegister, QuantumCircuit
from numpy import pi
from matplotlib import pyplot as plt
from qiskit import execute, Aer
#from qiskit.providers.aer import AerSimulator
from qiskit.visualization import plot_histogram
import numpy
'''
backend = Aer.get_backend('qasm_simulator')
theta_list=[]
probability_0_list=[]
probability_1_list=[]

for theta in numpy.arange( 0 , 2*pi , 2*pi/25 ) :
    circuit = QuantumCircuit(1, 1)
    circuit.h(0)
    circuit.p(theta , 0)
    circuit.h(0)
    circuit.measure([0],[0])
    job_sim = execute(circuit, backend, shots=1024)
    sim_result = job_sim.result()
    #print("sim_result=",sim_result) 
    counts=sim_result.get_counts(circuit)
    
    if '0' not in counts: #counts.gets('0', 0) == 0 :
        counts['0']=0
        #probability_1_list.insert(0,0)
    if '1' not in counts:
        counts['1']=0
        #probability_1_list.insert(0,0)

    probability_0 = counts['0']/1024
    probability_1 = counts['1']/1024

    #print("probability_0=", probability_0)
    #print("probability_1=", probability_1)

    probability_0_list.append(probability_0)
    probability_1_list.append(probability_1)

    

    print("counts['0']=", counts['0'])
    print("counts['1']=", counts['1'])

    
    theta_list.append(theta)

#circuit.draw('mpl')
print(circuit)

plt.plot(theta_list, probability_0_list,'ro',
         theta_list, probability_1_list,'bo')
plt.xlabel("theta", fontsize=13)
plt.ylabel("probability", fontsize=13)
'''
'''
theta_list=[]
probability_0_list=[]
probability_1_list=[]
backend = Aer.get_backend('qasm_simulator')

def circuit_test(n,theta):
    
    for i in numpy.arange( 0 , theta , theta/n) :
        circuit = QuantumCircuit(1, 1)
        circuit.h(0)
        circuit.p(i , 0)
        circuit.h(0)
        circuit.measure([0],[0])
        print(circuit)
        job_sim = execute(circuit, backend, shots=1024)
        sim_result = job_sim.result()
        counts=sim_result.get_counts(circuit)
        
        if '0' not in counts: #counts.gets('0', 0) == 0 :
            counts['0']=0
          
        if '1' not in counts:
            counts['1']=0
    
        probability_0 = counts['0']/1024
        probability_1 = counts['1']/1024
        
        print("counts['0']=", counts['0'])
        print("counts['1']=", counts['1'])
        #print("probability_0=", probability_0)
        #print("probability_1=", probability_1)
    
        probability_0_list.append(probability_0)
        probability_1_list.append(probability_1)
        theta_list.append(i)

print(circuit_test(25,2*pi))

plt.plot(theta_list, probability_0_list,'ro',
         theta_list, probability_1_list,'bo')
plt.xlabel("theta", fontsize=13)
plt.ylabel("probability", fontsize=13)

'''

theta_list=[]
probability_0_list=[]
probability_1_list=[]
backend = Aer.get_backend('qasm_simulator')

def circuit_test(n,theta):
    
    for i in numpy.arange( 0 , theta , theta/n) :
        circuit = QuantumCircuit(1, 1)
        circuit.h(0)
        circuit.p(i , 0)
        circuit.h(0)
        circuit.measure([0],[0])
        print(circuit)
        job_sim = execute(circuit, backend, shots=1024)
        sim_result = job_sim.result()
        counts=sim_result.get_counts(circuit)
        
        if '0' not in counts: #counts.gets('0', 0) == 0 :
            counts['0']=0
          
        if '1' not in counts:
            counts['1']=0
    
        probability_0 = counts['0']/1024
        probability_1 = counts['1']/1024
        
        print("counts['0']=", counts['0'])
        print("counts['1']=", counts['1'])
        #print("probability_0=", probability_0)
        #print("probability_1=", probability_1)
    
        probability_0_list.append(probability_0)
        probability_1_list.append(probability_1)
        theta_list.append(i)

print(circuit_test(25,2*pi))

plt.plot(theta_list, probability_0_list,'ro',
         theta_list, probability_1_list,'bo')
plt.xlabel("theta", fontsize=13)
plt.ylabel("probability", fontsize=13)

