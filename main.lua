function fact(i) 
    local x = i
    while i > 1 do
        i=i-1
        x = x * i
    end
    return x
end

i = 1000
while i > 0 do
    i=i-1
    x = 101
    while x > 0 do
        x = x - 1
        print((i+x) .. ". the factorial of " .. x .. " is " .. fact(x))
    end
end
