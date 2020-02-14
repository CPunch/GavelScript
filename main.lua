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
    x = 100
    while x > 0 do
        x = x - 1
        total = 1
        z = x
        while z > 1 do
            total = total * z
            z = z - 1
        end
    end
end
