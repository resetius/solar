import math
import random

N=999
M=4

print(-1)
print(N+M)

print("C1 0   0   0   0 0 0 10")
print("C2 0   0.1 0   0 0 0 1")
print("C3 0.1 0   0   0 0 0 1")
print("C4 0   0   0.1 0 0 0 1")

for i in range(N):
    m = 1.0
    theta = math.pi*random.random()
    phi = 2*math.pi*random.random()
    r = 1+random.random()*0.2-0.1
    x = r * math.sin(theta) * math.cos(phi)
    y = r * math.sin(theta) * math.sin(phi)
    z = r * math.cos(theta)

    print("B%d %f %f %f 0 0 0 %.16e" % (i, x, y, z, m))

for i in range(N):
    print("%d 000000 0.8 1.2 1.0" % (i+M))

print("0 00ff00 -1 0.8 2.5")
print("1 0000ff -1 0.8 2")
print("2 ff00ff -1 0.8 2")
print("3 00ffff -1 0.8 2")

