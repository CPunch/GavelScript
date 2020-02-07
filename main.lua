function fact(i) 
    local x = i
    while i > 1 do
        i=i-1
        x = x * i
    end
    return x
end

z = 1000

while z > 0 do
    z=z-1
    x = 0
    while x < 20 do
        x=x+1
        print("The factorial of " .. x .. " is " .. fact(x))
    end
end
