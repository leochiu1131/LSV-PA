import re
import sys
import numpy as np


def InputParse(file_name, pattern):
    cut_list = []
    with open(file_name, 'r', encoding='utf-8') as file:
        for line in file:
            line = line.strip()  
            if re.match(pattern, line):
                line = line.replace(':', '')
                line = line.split()
                line = list(map(int, line))
                cut_list.append(line)
                # print(line)

    cut_list = sorted(cut_list)
    print(file_name, 'total cut number:',len(cut_list))
    # print(cut_list)
    return cut_list


def main():
    file_A = sys.argv[1]
    file_B = sys.argv[2]

    pattern = r'^\d+:\s*\d+(\s+\d+)*$'

    list_A = InputParse(file_A, pattern)
    list_B = InputParse(file_B, pattern)

    print_out_A = []
    print_out_B = []

    offset_A  = 0
    offset_B = 0
    for i in range(max(len(list_A),len(list_B))):
        org_A = list_A[i+offset_A][:]
        org_B = list_B[i+offset_B][:]
        list_A[i + offset_A][1:] = sorted(list_A[i+offset_A][1:])
        list_B[i + offset_B][1:] = sorted(list_B[i+offset_B][1:])
        if list_A[i+offset_A] == list_B[i+offset_B]:
            pass
        elif (list_A[i+offset_A] > list_B[i+offset_B]):
            print_out_A.append(f'The cut in {file_B} : {list_B[i+offset_B]} can\'t be found in {file_A}')
            offset_A -= 1
        else:
            print_out_B.append(f'The cut in {file_A} : {list_A[i+offset_A]} can\'t be found in {file_B}')
            offset_B -= 1
        for j in range(1,len(org_A)-1):
            if(org_A[j] > org_A[j+1]):
                print_out_A.append(f'unsorted_cut in {file_A} : {org_A}')
                break
        for j in range(1,len(org_B)-1):
            if(org_B[j] > org_B[j+1]):
                print_out_B.append(f'unsorted_cut in {file_B} : {org_B}')
                break
        
    print_out_A = sorted(print_out_A)
    print_out_B = sorted(print_out_B)

    print("UNMATCHED OR INCORRECT CUTS IN : ", file_A)
    for line in print_out_A:
        print(line)

    print()
    print()

    print("UNMATCHED OR INCORRECT CUTS IN : ", file_B)
    for line in print_out_B:
        print(line)
    
    print()
    print()
    if (list_A == list_B):
        print('the same')
    else:
        print('different')


if __name__ == "__main__":
    main()