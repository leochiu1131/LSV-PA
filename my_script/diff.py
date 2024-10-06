import re
import numpy as np
import json


def InputParse(file_name, pattern):
    cut_list = []
    with open(file_name, 'r', encoding='utf-8') as file:
        for line in file:
            line = line.strip()  # 去除行首尾空白字符
            if re.match(pattern, line):
                line = line.replace(':', '')
                line = line.split()
                line = list(map(int, line))
                # line[1:] = sorted(line[1:]) 
                cut_list.append(line)
                # print(line)

    cut_list = sorted(cut_list)
    print(file_name, 'total cut number:',len(cut_list))
    # print(cut_list)
    return cut_list

# def InputParse(file_name, pattern):
#     cut_set = set()
#     with open(file_name, 'r', encoding='utf-8') as file:
#         for line in file:
#             line = line.strip()  # 去除行首尾空白字符
#             if re.match(pattern, line):
#                 line = line.replace(':', '')
#                 line = line.split()
#                 line = list(map(int, line))
#                 cut_set.add(tuple(line))

#     print(file_name, 'total cut number:',len(cut_set))
#     # print(cut_list)
#     return cut_set



file_A = '../bob_result/div6.out'
file_B = '../my_result/div6.out'

pattern = r'^\d+:\s*\d+(\s+\d+)*$'

list_A = InputParse(file_A, pattern)
list_B = InputParse(file_B, pattern)

result = ''

if (list_A == list_B):
    result = 'the same'
else:
    result = 'different'

print(result)

if result is 'different':
    for A, B in zip(list_A, list_B):
        if A != B:
            print(f'A: ( {A[0]}, {A[1:]})')
            print(f'B: ( {B[0]}, {B[1:]})')
            print('\n\n')
# with open("A.json", "w") as f:
#     json.dump(list_A, f)

# with open("B.json", "w") as f:
#     json.dump(list_B, f)


# set_A = InputParse(file_A, pattern)
# set_B = InputParse(file_B, pattern)

# if (set_A == set_B):
#     print('the same')
# else:
#     print('different')

# with open("A.txt", "w") as f:
#     f.write(str(set_A))

# with open("B.txt", "w") as f:
#     f.write(str(set_B))

