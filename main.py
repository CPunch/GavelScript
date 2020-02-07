def fact(i):
    x = i
    while i > 1:
        i=i-1
        x = x * i
    
    return x

z = 1000
while z > 0:
    z=z-1
    x=0
    while x < 100:
        x=x+1
        print("The factorial of " + str(x) + " is " + str(fact(x)))