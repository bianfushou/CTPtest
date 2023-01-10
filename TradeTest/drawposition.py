import matplotlib.pyplot as plt
import sys
fileName = sys.argv[1]

input = []
y = []
with open(fileName,"r") as file:
    line = file.readline()
    line = file.readline()
    i = 0
    while line:
        list = line.split(',')
        if list[12].strip()== 'W':
            input.append(100)
            y.append(i)
        if list[12].strip() == 'F':
            input.append(-100)
            y.append(i)
        i = i+1
        line = file.readline()
pv = 0
pf = 0
it = 0
pr = 0
for iff in input:
    if it > 0 :
        if iff == pr:
            pv = pv + 1
        else:
            pf = pf + 1
        pr = iff
    else :
        pr = iff
        it = 1
print(pv)
print(pf)
print(input)     
plt.plot(y, input)
plt.axis("equal")

plt.show()