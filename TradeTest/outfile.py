file = open("au2302.csv")
fout=open("au2302_out.csv", 'w+')

while True:
    data = file.readline()
    if data:
        spdata= data.split(',')
        fout.write("{},{},{}\n".format(spdata[0], spdata[14], spdata[9]))
    else:
        break
file.close()
fout.close()