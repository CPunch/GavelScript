def fact(i):
    x = i
    while i > 1:
        i=i-1
        x = x * i
    
    return x

for i in range(1000):
    for x in range(100):
        fact(x)