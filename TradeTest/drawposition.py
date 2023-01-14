import matplotlib.pyplot as plt
import sys
fileName = sys.argv[1]

input = []
y = []
shortP = [0,0]
LongP = [0,0]
with open(fileName,"r") as file:
    line = file.readline()
    line = file.readline()
    i = 0
    pflag = ""
    while line:
        list = line.split(',')
        if list[12].strip()== 'W':
            input.append(100)
            y.append(i)
            if pflag == '0':
                LongP[0] = LongP[0] + 1
            elif pflag == '1':
                shortP[0] = shortP[0] + 1
        if list[12].strip() == 'F':
            input.append(-100)
            y.append(i)
            if pflag == '0':
                LongP[1] = LongP[1] + 1
            elif pflag == '1':
                shortP[1] = shortP[1] + 1
        if list[12].strip() == 'N':
            pflag = list[5]
        i = i+1
        line = file.readline()
pv = 0
pf = 0
it = 0
pr = 0
print(LongP)
print(shortP)
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