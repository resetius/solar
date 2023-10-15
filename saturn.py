import math
import random

N=999
M=18666666.666666668

print(1)
print(2*N+1)
print("Saturn 0 0 0 0 0 0 %f" % (M)) 
for i in range(N):
    m = 1.0/N
    alpha = 2*math.pi*random.random()
    r = 1+random.random()*0.2-0.1
    x = r * math.cos(alpha)
    y = r * math.sin(alpha)

    vx = -y / r
    vy =  x / r

    vx = vx * math.sqrt(M/r)
    vy = vy * math.sqrt(M/r)

    print("B%d %f %f 0 %f %f 0 %.16e" % (i, x, y, vx, vy, m))

for i in range(N):
    m = 1.0/N
    alpha = 2*math.pi*random.random()
    r = 1.3+random.random()*0.2-0.1
    x = r * math.cos(alpha)
    y = r * math.sin(alpha)

    vx = -y / r
    vy =  x / r

    vx = vx * math.sqrt(M/r)
    vy = vy * math.sqrt(M/r)

    print("BB%d %f %f 0 %f %f 0 %.16e" % (i, x, y, vx, vy, m))
